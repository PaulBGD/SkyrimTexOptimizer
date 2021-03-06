#include <thread>
#include <iostream>
#include <sstream>
#include "processor.h"

using namespace std::chrono_literals;

//#define BUFFER_SIZE 1024 * 10

void processor(struct ThreadData data) {
    struct FileLocation input;
    NifFile nif;
    while (data.running->load() || !data.queue->empty()) {
        if (!data.queue->try_pop(input)) {
            std::this_thread::sleep_for(1ms);
            continue;
        }

        if (input.buffer.data == nullptr) {
            std::cout << "NULL??" << std::endl;
            continue;
        }

        std::string strinput(reinterpret_cast<char*>(input.buffer.data), input.buffer.size);
        std::stringstream ss(strinput);

        int error;
        if ((error = nif.Load(ss))) {
            std::wcerr << "thread #" << data.threadNum << " failed to handle " << input.path << ": " << error << std::endl;
            delete (char*) input.buffer.data;
            input.buffer.data = nullptr;
            continue;
        }

        // calculate size
        for (const auto &shape : nif.GetShapes()) {
            NiShader *shader = nif.GetShader(shape);
            if (shader) {
                BoundingSphere sphere = shape->GetBounds();
                std::vector<std::string> texFiles;
                for (int i = 0; i < 20; i++) {
                    std::string texture;
                    nif.GetTextureSlot(shader, texture, i);
                    if (!texture.empty()) {
                        texFiles.push_back(texture);
                        if (sphere.radius > (*data.sizes)[texture].size) {
                            struct SizeData sizeData{
                                sphere.radius,
                                input.path
                            };
                            (*data.sizes)[texture] = sizeData;
                        }
                    }
                }
            }
        }

        delete (char*) input.buffer.data;
        input.buffer.data = nullptr;
    }
}

//void processor(struct ThreadData data) {
//    TexturesOptimizer opt;
//    char buffer[BUFFER_SIZE];
//
//    while (*data.running || !data.queue->empty()) {
//        struct InputFile file;
//        if (!data.queue->try_pop(file)) {
//            std::this_thread::sleep_for(500ms);
//            continue;
//        }
//
//        std::cout << data.threadNum << ": " << "Checking previous hash" << std::endl;
//        std::string inputHash;
//        std::filesystem::path infoFile = std::filesystem::path(file.output).concat(".info.mohidden");
//        if (std::filesystem::exists(infoFile)) {
//            std::ifstream infoIn(infoFile);
//            std::stringstream buf;
//            buf << infoIn.rdbuf();
//
//            std::string fileContents = buf.str();
//            size_t middleIndex = fileContents.find(':');
//            if (middleIndex != std::string::npos) {
//                std::string previousHash = fileContents.substr(0, middleIndex);
//                uint32_t previousSize = atoi(fileContents.substr(middleIndex + 1).c_str());
//
//                std::ifstream in(file.input);
//                Chocobo1::SHA2_512_256 sha2;
//
//                if (in.is_open()) {
//                    long long read = 0;
//                    while ((read = in.readsome(buffer, BUFFER_SIZE)) > 0) {
//                        sha2.addData(buffer, read);
//                    }
//                }
//                inputHash = sha2.toString();
//
//                if (inputHash == previousHash && previousSize == file.size) {
//                    std::cout << "Skipping " << file.entry << std::endl;
//                    continue;
//                }
//            }
//        }
//
//        std::cout << data.threadNum << ": " << "Opening" << std::endl;
//        if (!opt.open(file.input.string(), TexturesOptimizer::TextureType::DDS)) {
//            std::cerr << "Failed to open " << file.entry << std::endl;
//            continue;
//        }
//        std::cout << data.threadNum << ": " << "Doing work" << std::endl;
//        auto info = opt.getInfo();
//        size_t previousHeight = info.height, previousWidth = info.width;
//        if (!opt.doCPUWork(file.size, file.size)) {
//            std::cerr << "Failed to do CPU work for " << file.entry << std::endl;
//            continue;
//        }
//        std::cout << data.threadNum << ": " << "Doing GPU work" << std::endl;
//        if (!opt.doGPUWork(0)) {
//            std::cerr << "Failed to do GPU work for " << file.entry << std::endl;
//            continue;
//        }
//
//        std::cout << data.threadNum << ": " << "Saving" << std::endl;
//        std::filesystem::path outputDirectory = file.output.parent_path();
//        if (!std::filesystem::exists(outputDirectory)) {
//            std::error_code ec;
//            std::filesystem::create_directories(outputDirectory, ec);
//            if (ec) {
//                std::cerr << "Error creating directories " << outputDirectory << ": " << ec.message() << std::endl;
//                continue;
//            }
//        }
//
//        if (!opt.saveToFile(file.output.string())) {
//            std::cerr << "Failed to save " << file.output << std::endl;
//            continue;
//        }
//        std::cout << data.threadNum << ": " << "Processing hash" << std::endl;
//
//        if (inputHash.empty()) {
//            std::ifstream in(file.input);
//            Chocobo1::SHA2_512_256 sha2;
//
//            if (in.is_open()) {
//                long long read;
//                while ((read = in.readsome(buffer, BUFFER_SIZE)) > 0) {
//                    sha2.addData(buffer, read);
//                }
//            }
//            inputHash = sha2.toString();
//        }
//
//        std::ofstream outInfo(infoFile);
//        outInfo << inputHash << ":" << file.size;
//        outInfo.close();
//
//        std::cout << "Processed " << file.entry << " on thread #" << data.threadNum << "!";
//        info = opt.getInfo();
//        std::cout << " Resized from " << previousWidth << "/" << previousHeight << " to " << info.width << "/"
//                  << info.height << std::endl;
//    }
//}
//
