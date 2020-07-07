#define UNICODE 1
#define _UNICODE 1

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include "main.h"

#include "libbsarch.h"
#include "processor.h"
#include "resizer.h"
#include "sha2_512_256.h"

#include <iostream>
#include <chrono>
#include <filesystem>
#include <concurrent_queue.h>
#include <concurrent_unordered_map.h>
#include <thread>

#include <windows.h>
#include <shlobj.h>
#include <sstream>
#include <unordered_map>

static bool hasEnding(std::wstring const &fullString, std::wstring const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

inline bool isValidNIF(std::wstring const &str) {
    return hasEnding(str, L".nif") &&
           str.find(L"\\lod\\") == std::string::npos;
}

inline bool isValidTexture(std::string const &str) {
    return str.find(R"(textures\effects\gradients\)") == std::string::npos &&
           str.find("textures\\lod\\") == std::string::npos;
}

static std::vector<std::string> parseBSAList(const std::string &str) {
    std::vector<std::string> list;

    std::string delim = ", ";
    auto start = 0U;
    auto end = str.find(delim);
    while (end != std::string::npos) {
        list.push_back(str.substr(start, end - start));
        start = end + delim.length();
        end = str.find(delim, start);
    }
    if (str.length()) {
        list.push_back(str.substr(start, end - start));
    }
    return list;
}

int main(int argc, char **argv) {
    if (argc < 5) {
        std::cerr << "Usage: skyrimtexoptimizer <output> <texsize> <normalsize>" << std::endl;
        return 1;
    }
    auto input = std::filesystem::absolute(argv[1]);
    auto output = std::filesystem::absolute(argv[2]);
    uint32_t texsize = atoi(argv[3]);
    uint32_t normalsize = atoi(argv[4]);

    std::cout << "input: " << input << " output: " << output << " texsize: " << texsize << " normalsize: " << normalsize
              << std::endl;

    auto running = new std::atomic<bool>(true);

    int cores = 24;
    std::vector<std::thread> threads;
    std::vector<struct ThreadData> threadData;
    for (int i = 0; i < cores; i++) {
        auto queue = new concurrency::concurrent_queue<struct FileLocation>();
        auto map = new std::unordered_map<std::string, struct SizeData>();

        struct ThreadData data{
                queue,
                map,
                i,
                running
        };
        threadData.emplace_back(data);

        threads.emplace_back(std::thread(processor, data));
    }

    std::vector<std::filesystem::path> archives;

    {
        // start by loading bsa list
        PWSTR path; // should free this but ehhhh
        HRESULT result = SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_CREATE, nullptr, &path);
        if (FAILED(result)) {
            std::cerr << "Failed to find Documents folder: " << std::system_category().message(result) << std::endl;
            return 1;
        }
        std::wstring knownFolder(path);
        std::filesystem::path skyrimIniLoc(
                std::filesystem::path(knownFolder).append("My Games").append("Skyrim Special Edition").append(
                        "Skyrim.ini"));
        std::ifstream skyrimIniIn(skyrimIniLoc);
        std::string line;

        while (std::getline(skyrimIniIn, line)) {
            std::string remainder;
            if (line.rfind("sResourceArchiveList2", 0) == 0) {
                remainder = line.substr(strlen("sResourceArchiveList2="));
            } else if (line.rfind("sResourceArchiveList", 0) == 0) {
                remainder = line.substr(strlen("sResourceArchiveList="));
            }
            if (remainder.length()) {
                auto list = parseBSAList(remainder);
                for (std::string &str : list) {
                    archives.push_back(std::filesystem::current_path().append("data").append(str));
                }
            }
        }
    }

    // get the %appdata%/local folder
    wchar_t buffer[MAX_PATH];
    BOOL success = SHGetSpecialFolderPath(nullptr, buffer, CSIDL_LOCAL_APPDATA, false);
    if (!success) {
        std::cerr << "Failed to find appdata folder" << std::endl;
        return 1;
    }
    std::filesystem::path loadOrderFile = std::filesystem::absolute(
            std::wstring(buffer) + L"\\Skyrim Special Edition\\loadorder.txt");
    std::ifstream loadOrderIn(loadOrderFile);
    std::string line;

    while (std::getline(loadOrderIn, line)) {
        std::filesystem::path fileLocation = std::filesystem::current_path().append("data").append(
                line).replace_filename(".bsa");
        if (std::filesystem::exists(fileLocation)) {
            archives.push_back(fileLocation);
        }
    }

    // lowercase path ex meshes\\path\\somemesh to the location
    //   because we iterate over the BSAs in order, then over the
    //   lose files it should be good
    std::unordered_map<std::wstring, FileLocation> meshes;

    // todo load BSAs on multiple threads, would save a lot of time given there's like 10 BSAs base alone
    for (std::filesystem::path &path : archives) {
        if (!std::filesystem::exists(path)) {
            std::wcerr << L"Failed to find BSA " << path << std::endl;
            return 1;
        }
        std::wcout << "Loading BSA " << path.filename() << std::endl;

        std::ifstream in(path);
        Chocobo1::SHA2_512_256 sha2;
#define BUFFER_SIZE 1024 * 10
        char readBuf[BUFFER_SIZE];

        if (in.is_open()) {
            long long read = 0;
            while ((read = in.readsome(readBuf, BUFFER_SIZE)) > 0) {
                sha2.addData(readBuf, read);
            }
        }

        auto dataPath = std::filesystem::path(output).append(path.filename().string() + ".data.mohidden");
        if (std::filesystem::exists(dataPath)) {

        }

        bsa_result_message_t result = {0};
        bsa_archive_t archive = bsa_create();

        result = bsa_load_from_file(archive, path.wstring().c_str());
        if (result.code < 0) {
            std::wcout << "Failed to load BSA " << result.text << std::endl;
            return 1;
        }

        bsa_entry_list_t entries = bsa_entry_list_create();
        bsa_get_resource_list(archive, entries, L"");

        for (size_t index = 0; index < bsa_entry_list_count(entries); index++) {
            wchar_t filename[2048];
            bsa_entry_list_get(entries, index, 2048, filename);

            auto internalPath = std::wstring(filename);
            if (isValidNIF(internalPath)) {
                bsa_result_message_buffer_t buf = bsa_extract_file_data_by_filename(archive, filename);
                result = buf.message;
                if (result.code < 0) {
                    std::wcout << "Failed to load BSA " << result.text << std::endl;
                    return 1;
                }
                bsa_result_buffer_t resultBuf = buf.buffer;
                auto copy = new char[buf.buffer.size];
                memcpy_s(copy, buf.buffer.size, buf.buffer.data, buf.buffer.size);
                bsa_file_data_free(archive, resultBuf);

                bsa_result_buffer_t resultBuffer{
                        buf.buffer.size,
                        (bsa_buffer_t) copy
                };

                struct FileLocation loc{
                        internalPath,
                        resultBuffer
                };
                meshes.insert(std::make_pair(internalPath, loc));
            }
        }
        bsa_entry_list_free(entries);
        bsa_close(archive);
    }

    // now load filesystem meshes
    auto dataDir = std::filesystem::current_path().append("data");
    std::filesystem::recursive_directory_iterator iter(dataDir);
    std::filesystem::recursive_directory_iterator end;

    while (iter != end) {
        if (iter->is_regular_file()) {
            std::filesystem::path path = iter->path();

            std::wstring filepath = path.wstring();
            transform(filepath.begin(), filepath.end(), filepath.begin(), ::tolower);

            if (isValidNIF(filepath)) {

                std::ifstream in(path);
                std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                auto copy = new char[contents.size()];
                memcpy_s(copy, contents.size(), contents.c_str(), contents.size());
                bsa_result_buffer_t resultBuffer{
                        static_cast<uint32_t>(contents.size()),
                        (bsa_buffer_t) copy
                };

                auto internalPath = std::wstring(path.lexically_relative(dataDir));
                std::transform(internalPath.begin(), internalPath.end(), internalPath.begin(), ::tolower);
                struct FileLocation loc{
                        internalPath,
                        resultBuffer
                };
                meshes.insert(std::make_pair(internalPath, loc));
            }
        }

        std::error_code code;
        iter.increment(code);

        if (code) {
            std::cerr << "Error While Accessing : " << iter->path().string() << " :: " << code.message() << std::endl;
        }
    }

    std::cout << "Loaded " << meshes.size() << " meshes" << std::endl;

    int millis = 0;

    while (!meshes.empty()) {
        for (struct ThreadData &data : threadData) {
            if (data.queue->empty()) {
                // give it 25 items
                for (int i = 0; i < 25 && !meshes.empty(); i++) {
                    auto elem = meshes.begin();
                    struct FileLocation loc = elem->second;
                    meshes.erase(elem);
                    data.queue->push(loc);
                }
            }
        }

        if (++millis % 1000 == 0) {
            std::cout << "Meshes left: " << meshes.size() << std::endl;
        }

        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1ms);
    }

    running->store(false);
    std::cout << "Waiting on thread.. " << std::endl;
    for (auto &th : threads) {
        if (th.joinable()) {
            th.join();
        }
    }
    threads.clear();

    std::unordered_map<std::string, struct SizeData> finalMap;

    for (const auto &data : threadData) {
        for (const auto &value : *data.sizes) {
            if (value.second.size > 0 && value.second.size > finalMap[value.first].size) {
                if (isValidTexture(value.first)) {
                    if (value.first.find("lod") != std::string::npos) {
                        std::cout << value.first << "lod" << std::endl;
                    }
                    finalMap[value.first] = value.second;
                }
            }
        }
    }
    std::cout << "Texture Count: " << finalMap.size() << std::endl;
    std::cout << "Checking filesystem for textures.." << std::endl;

    std::unordered_map<std::string, GameResource *> resources;

    for (const auto &value : finalMap) {
        auto file = std::filesystem::current_path().append("data").append(value.first);

        if (std::filesystem::exists(file)) {
            if (resources[value.first]) {
                delete resources[value.first];
            }
            resources[value.first] = new FileSystemResource(file, value.second.size, value.second.mesh);
        }
    }

    std::wcout << "Loading BSAs again for textures" << std::endl;
    for (std::filesystem::path &path : archives) {
        if (!std::filesystem::exists(path)) {
            std::wcerr << L"Failed to find BSA " << path << std::endl;
            return 1;
        }
        bsa_result_message_t result = {0};
        bsa_archive_t archive = bsa_create();

        result = bsa_load_from_file(archive, path.wstring().c_str());
        if (result.code < 0) {
            std::wcout << "Failed to load BSA " << result.text << std::endl;
            return 1;
        }

        for (const auto &value : finalMap) {
            std::wstring wide = shortToWide(value.first);
            bsa_file_record_t record = bsa_find_file_record(archive, wide.c_str());
            if (record) {
                auto existing = resources[value.first];
                if (!existing || dynamic_cast<BSAResource *>(existing) != nullptr) {
                    // only override if it's overriding a BSAResource, as filesystem takes prio
                    if (existing) {
                        delete resources[value.first];
                    }
//                    resources[value.first] = new BSAResource(wide, value.second, &archive);
                    bsa_result_message_buffer_t buf = bsa_extract_file_data_by_record(archive, record);
                    result = buf.message;
                    if (result.code < 0) {
                        std::wcout << "Failed to load BSA " << result.text << std::endl;
                        return 1;
                    }
                    auto copy = new char[buf.buffer.size];
                    memcpy_s(copy, buf.buffer.size, buf.buffer.data, buf.buffer.size);
                    struct GameData data{
                            copy,
                            buf.buffer.size
                    };
                    bsa_file_data_free(archive, buf.buffer);
                    resources[value.first] = new BSAResource(data, value.second.size, value.second.mesh);
                }
            }
        }

        bsa_close(archive);
    }

    std::cout << "Done processing textures, starting resizing.." << std::endl;

    running->store(true);

    std::vector<struct ResizeData> resizeData;
    for (int i = 0; i < 1; i++) {
        auto queue = new concurrency::concurrent_queue<struct TextureData>();

        struct ResizeData data{
                queue,
                output,
                running
        };
        resizeData.emplace_back(data);

        threads.emplace_back(std::thread(resizer, data));
    }

    while (!resources.empty()) {
        for (auto &thread : resizeData) {
            if (thread.queue->empty()) {
                // give it a few items
                for (int i = 0; i < 2 && !resources.empty(); i++) {
                    auto elem = resources.begin();
                    struct TextureData data{
                            elem->first,
                            elem->second,
                    };
                    resources.erase(elem);
                    thread.queue->push(data);
                }
            }
        }

        if (++millis % 1000 == 0) {
            std::cout << "Textures left: " << resources.size() << std::endl;
        }

        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1ms);
    }

    running->store(false);
    std::cout << "Waiting on thread.. " << std::endl;
    for (auto &th : threads) {
        if (th.joinable()) {
            th.join();
        }
    }

    std::cout << "Finished" << std::endl;

    return 0;
}
