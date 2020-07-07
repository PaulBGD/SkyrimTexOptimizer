#ifndef STO_PROCESSOR_H
#define STO_PROCESSOR_H

#include <filesystem>
#include <concurrent_queue.h>
#include <variant>
#include <bs_archive.h>
#include "libs/NIF/NifFile.h"

enum LocationType {LOCATION_TYPE_ARCHIVE, LOCATION_TYPE_PATH};

struct FileLocation {
    std::wstring path;
    bsa_result_buffer_t buffer;
};

struct InputFile {
    std::filesystem::path input;
    std::filesystem::path output;
    std::string entry;
    uint32_t size;
};

struct SizeData {
    float size;
    std::wstring mesh;
};

struct ThreadData {
    concurrency::concurrent_queue<struct FileLocation> *queue;
    std::unordered_map<std::string, struct SizeData> *sizes;
    int threadNum;
    std::atomic<bool>* running;
};

void processor(struct ThreadData data);

#endif //STO_PROCESSOR_H
