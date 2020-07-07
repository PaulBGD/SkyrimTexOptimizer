#ifndef STO_MESHBSA_H
#define STO_MESHBSA_H

#include <filesystem>
#include <utility>
#include "main.h"

class MeshBSA {
private:
    std::filesystem::path bsaPath;
    struct OptimizerSettings settings;
public:
    explicit MeshBSA(std::filesystem::path& bsaPath, struct OptimizerSettings settings) {
        this->bsaPath = std::move(bsaPath);
        this->settings = std::move(settings);
    }
};

#endif //STO_MESHBSA_H
