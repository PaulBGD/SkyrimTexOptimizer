#include <thread>
#include "resizer.h"
#include "textures.hpp"
#include "sha2_512_256.h"

using namespace std::chrono_literals;

inline bool hasEnding(std::string const &fullString, std::string const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

void resizer(const struct ResizeData &data) {
    struct TextureData texture;
    while (data.running->load() || !data.queue->empty()) {
        if (!data.queue->try_pop(texture)) {
            std::this_thread::sleep_for(1ms);
            continue;
        }
        auto resource = texture.resource->getData();
        auto output = std::filesystem::path(data.output_dir).append(texture.path);
        std::string inputHash;
        std::filesystem::path infoFile = std::filesystem::path(output).concat(".info.mohidden");

        TexturesOptimizer opt;
        if (!opt.read(texture.path, resource->data, resource->length, TexturesOptimizer::TextureType::DDS)) {
            std::cerr << "Failed to open " << texture.path << std::endl;
            continue;
        }
        auto info = opt.getInfo();
        size_t previousHeight = info.height, previousWidth = info.width;
        size_t neededSize = std::max<size_t>(((size_t) texture.resource->size) << 4u, 128);

        // http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
        neededSize--;
        neededSize |= neededSize >> 1u;
        neededSize |= neededSize >> 2u;
        neededSize |= neededSize >> 4u;
        neededSize |= neededSize >> 8u;
        neededSize |= neededSize >> 16u;
        neededSize |= neededSize >> 32u;
        neededSize++;

        neededSize = std::min<size_t>(neededSize, previousWidth);

        if (hasEnding(texture.path, "_n.dds")) {
            neededSize >>= 2u;
        }

        if (neededSize < 128) { // only resize below 128 if the original is such
            neededSize = std::max<size_t>(neededSize, previousWidth);
        }

        if (std::filesystem::exists(infoFile)) {
            std::ifstream infoIn(infoFile);
            std::stringstream buf;
            buf << infoIn.rdbuf();

            std::string fileContents = buf.str();
            size_t middleIndex = fileContents.find(':');
            if (middleIndex != std::string::npos) {
                std::string previousHash = fileContents.substr(0, middleIndex);
                uint32_t previousSize = atoi(fileContents.substr(middleIndex + 1).c_str());

                Chocobo1::SHA2_512_256 sha2;
                sha2.addData(resource->data, resource->length);
                inputHash = sha2.toString();

                if (inputHash == previousHash && previousSize == neededSize) {
                    continue;
                }
            }
        }


        std::wcout << "Previous height: " << previousHeight << " new height: " << neededSize;
        std::wcout << " Previous width: " << previousWidth << " new width: " << neededSize << " " << texture.path.c_str() << " from: " << texture.resource->mesh << std::endl;
        if (!opt.doCPUWork(neededSize, neededSize)) {
            std::cerr << "Failed to do CPU work for " << texture.path << std::endl;
            continue;
        }
        if (!opt.doGPUWork(0)) {
            std::cerr << "Failed to do GPU work for " << texture.path << std::endl;
            continue;
        }

        std::filesystem::path outputDirectory = output.parent_path();
        if (!std::filesystem::exists(outputDirectory)) {
            std::error_code ec;
            std::filesystem::create_directories(outputDirectory, ec);
            if (ec) {
                std::cerr << "Error creating directories " << outputDirectory << ": " << ec.message() << std::endl;
                continue;
            }
        }

        if (!opt.saveToFile(output.string())) {
            std::cerr << "Failed to save " << output << std::endl;
            continue;
        }

        if (inputHash.empty()) {
            Chocobo1::SHA2_512_256 sha2;
            sha2.addData(resource->data, resource->length);
            inputHash = sha2.toString();
        }

        std::ofstream outInfo(infoFile);
        outInfo << inputHash << ":" << resource->length;
        outInfo.close();
    }
}
