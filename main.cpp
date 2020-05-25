#include <iostream>
#include <chrono>
#include <filesystem>
#include <concurrent_queue.h>
#include <thread>
#include <fstream>
#include <sstream>
#include "textures.hpp"
#include "sha2_512_256.h"

static bool hasEnding(std::wstring const &fullString, std::wstring const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

static bool running = true;

using namespace std::chrono_literals;

struct InputFile {
    std::filesystem::path input;
    std::filesystem::path output;
    std::string entry;
    uint32_t size;
};

struct ThreadData {
    concurrency::concurrent_queue<struct InputFile> *queue;
    int threadNum;
};

#define BUFFER_SIZE 1024 * 10

void processor(struct ThreadData data) {
    TexturesOptimizer opt;
    char buffer[BUFFER_SIZE];

    while (running || !data.queue->empty()) {
        struct InputFile file;
        if (!data.queue->try_pop(file)) {
            std::this_thread::sleep_for(500ms);
            continue;
        }

        std::cout << data.threadNum << ": " << "Checking previous hash" << std::endl;
        std::string inputHash;
        std::filesystem::path infoFile = std::filesystem::path(file.output).concat(".info.mohidden");
        if (std::filesystem::exists(infoFile)) {
            std::ifstream infoIn(infoFile);
            std::stringstream buf;
            buf << infoIn.rdbuf();

            std::string fileContents = buf.str();
            size_t middleIndex = fileContents.find(':');
            if (middleIndex != std::string::npos) {
                std::string previousHash = fileContents.substr(0, middleIndex);
                uint32_t previousSize = atoi(fileContents.substr(middleIndex + 1).c_str());

                std::ifstream in(file.input);
                Chocobo1::SHA2_512_256 sha2;

                if (in.is_open()) {
                    long long read = 0;
                    while ((read = in.readsome(buffer, BUFFER_SIZE)) > 0) {
                        sha2.addData(buffer, read);
                    }
                }
                inputHash = sha2.toString();

                if (inputHash == previousHash && previousSize == file.size) {
                    std::cout << "Skipping " << file.entry << std::endl;
                    continue;
                }
            }
        }

        std::cout << data.threadNum << ": " << "Opening" << std::endl;
        if (!opt.open(file.input.string(), TexturesOptimizer::TextureType::DDS)) {
            std::cerr << "Failed to open " << file.entry << std::endl;
            continue;
        }
        std::cout << data.threadNum << ": " << "Doing work" << std::endl;
        auto info = opt.getInfo();
        size_t previousHeight = info.height, previousWidth = info.width;
        if (!opt.doCPUWork(file.size, file.size)) {
            std::cerr << "Failed to do CPU work for " << file.entry << std::endl;
            continue;
        }
        std::cout << data.threadNum << ": " << "Doing GPU work" << std::endl;
        if (!opt.doGPUWork(0)) {
            std::cerr << "Failed to do GPU work for " << file.entry << std::endl;
            continue;
        }

        std::cout << data.threadNum << ": " << "Saving" << std::endl;
        std::filesystem::path outputDirectory = file.output.parent_path();
        if (!std::filesystem::exists(outputDirectory)) {
            std::error_code ec;
            std::filesystem::create_directories(outputDirectory, ec);
            if (ec) {
                std::cerr << "Error creating directories " << outputDirectory << ": " << ec.message() << std::endl;
                continue;
            }
        }

        if (!opt.saveToFile(file.output.string())) {
            std::cerr << "Failed to save " << file.output << std::endl;
            continue;
        }
        std::cout << data.threadNum << ": " << "Processing hash" << std::endl;

        if (inputHash.empty()) {
            std::ifstream in(file.input);
            Chocobo1::SHA2_512_256 sha2;

            if (in.is_open()) {
                long long read = 0;
                while ((read = in.readsome(buffer, BUFFER_SIZE)) > 0) {
                    sha2.addData(buffer, read);
                }
            }
            inputHash = sha2.toString();
        }

        std::ofstream outInfo(infoFile);
        outInfo << inputHash << ":" << file.size;
        outInfo.close();

        std::cout << "Processed " << file.entry << " on thread #" << data.threadNum << "!";
        info = opt.getInfo();
        std::cout << " Resized from " << previousWidth << "/" << previousHeight << " to " << info.width << "/" << info.height << std::endl;
    }
}

int main(int argc, char **argv) {
    if (argc < 5) {
        std::cerr << "Usage: skyrimtexoptimizer <input> <output> <texsize> <normalsize>" << std::endl;
        return 1;
    }
    auto input = std::string(argv[1]);
    auto output = std::filesystem::absolute(argv[2]);
    uint32_t texsize = atoi(argv[3]);
    uint32_t normalsize = atoi(argv[4]);

    std::cout << "input: " << input << " output: " << output << " texsize: " << texsize << " normalsize: " << normalsize
              << std::endl;

    std::filesystem::recursive_directory_iterator iter(input);
    std::filesystem::recursive_directory_iterator end;

    concurrency::concurrent_queue<struct InputFile> queue;

    int cores = 8;
    std::vector<std::thread> threads(cores);
    for (int i = 0; i < cores; i++) {
        struct ThreadData data{
                &queue,
                i
        };

        threads.emplace_back(std::thread([data]() {
            processor(data);
        }));
        std::cout << "Started thread " << i << std::endl;
    }

    while (iter != end) {
        if (iter->is_regular_file()) {
            std::filesystem::path path;
            bool good = false;
            try {
                path = iter->path();
                good = true;
            } catch (const std::exception &exc) {
                std::cerr << "Exception:" << exc.what() << std::endl;
            } catch (...) {
                std::cerr << "Unknown Exception" << std::endl;
            }

            if (good) {
                std::wstring filepath = path.wstring();
                transform(filepath.begin(), filepath.end(), filepath.begin(), ::tolower);

                if (
                        filepath.find(L"\\lod\\") == std::string::npos &&
                        filepath.find(L"\\textures\\dyndolod\\") == std::string::npos &&
                        hasEnding(filepath, L".dds")
                        ) {
                    uint32_t size = texsize;
                    if (hasEnding(filepath, L"_n.dds")) {
                        size = normalsize;
                    }
                    std::filesystem::path relative = path.lexically_relative(input);
                    struct InputFile file{
                            path,
                            std::filesystem::path(output).append(relative.string()),
                            relative.string(),
                            size
                    };
                    queue.push(file);
                }
            }
        }

        std::error_code code;
        iter.increment(code);

        if (code) {
            std::cerr << "Error While Accessing : " << iter->path().string() << " :: " << code.message() << std::endl;
        }
    }

    running = false;
    for (auto &th : threads) {
        if (th.joinable()) {
            th.join();
        }
    }

    std::cout << "Finished" << std::endl;

    return 0;
}
