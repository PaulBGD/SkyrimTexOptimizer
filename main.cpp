#include <iostream>
#include <chrono>
#include <filesystem>
#include <concurrent_queue.h>
#include <thread>
#include "processor.h"

static bool hasEnding(std::wstring const &fullString, std::wstring const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
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

    bool running = true;

    int cores = 8;
    std::vector<std::thread> threads(cores);
    for (int i = 0; i < cores; i++) {
        struct ThreadData data{
                &queue,
                i,
                &running
        };

        threads.emplace_back(std::thread(processor, data));
        std::cout << "Started thread " << i << std::endl;
    }

    while (iter != end) {
        if (iter->is_regular_file()) {
            std::filesystem::path path = iter->path();

            std::wstring filepath = path.wstring();
            transform(filepath.begin(), filepath.end(), filepath.begin(), ::tolower);

            if (
                    filepath.find(L"\\lod\\") == std::string::npos && // lods in lod folder
                    filepath.find(L"\\textures\\dyndolod\\") == std::string::npos && // lods in dyndolod folder
                    std::count(filepath.begin(), filepath.end(), L'.') < 4 && // lods that have x.y.z.dds format
                    hasEnding(filepath, L".dds") // and finally check that it's a texture
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
