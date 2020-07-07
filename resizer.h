#ifndef STO_RESIZER_H
#define STO_RESIZER_H

#include "main.h"

#include <string>
#include <concurrent_queue.h>
#include <filesystem>

struct TextureData {
    std::string path;
    GameResource *resource;
};

struct ResizeData {
    concurrency::concurrent_queue<struct TextureData> *queue;
    std::filesystem::path output_dir;
    std::atomic<bool>* running;
};

void resizer(const struct ResizeData& data);

#endif //STO_RESIZER_H
