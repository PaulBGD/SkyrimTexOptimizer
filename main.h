#ifndef STO_MAIN_H
#define STO_MAIN_H

#include <bs_archive.h>
#include <string>
#include <utility>
#include <fstream>
#include <sstream>
#include <filesystem>

inline std::wstring shortToWide(const std::string &str) {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring wide = converter.from_bytes(str);
    return wide;
}

struct OptimizerSettings {
    std::filesystem::path outputDirectory;
    std::filesystem::path inputDirectory;
};

struct GameData {
    char *data;
    size_t length;
};

class GameResource {
public:
    virtual GameData *getData() = 0;

    virtual void freeData(GameData *data) = 0;

    explicit GameResource(float size, std::wstring mesh) {
        this->size = size;
        this->mesh = std::move(mesh);
    }

public:
    float size;
    std::wstring mesh;
};

class BSAResource : public GameResource {
private:
    GameData gameData;
    std::wstring path;
public:
    BSAResource(GameData gameData, float size, std::wstring mesh) : GameResource(size, std::move(mesh)) {
//        this->path = std::move(path);
        this->gameData = gameData;
    }

    GameData *getData() override {
//        bsa_result_message_t result = {0};
//        bsa_archive_t archive = bsa_create();
//
//        std::wcout << "loading" << this->archivePath << std::endl;
//        result = bsa_load_from_file(archive, this->archivePath.wstring().c_str());
//        if (result.code < 0) {
//            std::wcerr << "Failed to load BSA " << result.text << std::endl;
//            return nullptr;
//        }
//
//        auto data = bsa_extract_file_data_by_filename(archive, this->path.c_str());
//        result = data.message;
//        if (result.code < 0) {
//            std::wcerr << "Failed to load BSA " << result.text << std::endl;
//            return nullptr;
//        }
//        auto copy = new char[data.buffer.size];
//        memcpy_s(copy, data.buffer.size, data.buffer.data, data.buffer.size);
//
//        bsa_close(archive);
//
//        return new GameData{
//                copy,
//                data.buffer.size
//        };
        return &this->gameData;
    }

    void freeData(GameData *data) override {
        delete data->data;
//        delete data;
    }
};

class FileSystemResource : public GameResource {
private:
    std::filesystem::path path;
public:
    FileSystemResource(std::filesystem::path path, float size, std::wstring mesh) : GameResource(size, mesh) {
        this->path = std::move(path);
    }

    GameData *getData() override {
        std::ifstream in(this->path);
        std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

        auto copy = new char[contents.size()];
        memcpy_s(copy, contents.size(), contents.c_str(), contents.size());

        return new GameData{
                copy,
                contents.size()
        };
    }

    void freeData(GameData *data) override {
        delete data->data;
        delete data;
    }
};

#endif //STO_MAIN_H
