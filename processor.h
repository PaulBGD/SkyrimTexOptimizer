#ifndef STO_PROCESSOR_H
#define STO_PROCESSOR_H

#include <filesystem>
#include <concurrent_queue.h>

struct InputFile {
    std::filesystem::path input;
    std::filesystem::path output;
    std::string entry;
    uint32_t size;
};

struct ThreadData {
    concurrency::concurrent_queue<struct InputFile> *queue;
    int threadNum;
    bool* running;
};

void processor(struct ThreadData data);

#endif //STO_PROCESSOR_H
