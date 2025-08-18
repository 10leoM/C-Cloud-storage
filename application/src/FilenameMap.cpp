#include "FilenameMap.h"
#include <nlohmann/json.hpp>
#include <experimental/filesystem>
#include <fstream>
#include "Logger.h"

namespace fs = std::experimental::filesystem;
using json = nlohmann::json;

FilenameMap::FilenameMap(const std::string &mappingFile) : mappingFile_(mappingFile) {}

void FilenameMap::load() {
    std::lock_guard<std::mutex> lk(mtx_);
    loadInternal();
}

void FilenameMap::save() {
    std::lock_guard<std::mutex> lk(mtx_);
    saveInternal();
}

void FilenameMap::add(const std::string &serverFilename, const std::string &originalFilename) {
    std::lock_guard<std::mutex> lk(mtx_);
    loadInternal();
    mapping_[serverFilename] = originalFilename;
    saveInternal();
}

std::string FilenameMap::getOriginal(const std::string &serverFilename) {
    std::lock_guard<std::mutex> lk(mtx_);
    loadInternal();
    auto it = mapping_.find(serverFilename);
    return it != mapping_.end() ? it->second : serverFilename;
}

std::map<std::string, std::string> FilenameMap::getAll() {
    std::lock_guard<std::mutex> lk(mtx_);
    loadInternal();
    return mapping_;
}

void FilenameMap::erase(const std::string &serverFilename) {
    std::lock_guard<std::mutex> lk(mtx_);
    loadInternal();
    mapping_.erase(serverFilename);
    saveInternal();
}

void FilenameMap::loadInternal() {
    try{
        if(fs::exists(mappingFile_)){
            std::ifstream file(mappingFile_);
            mapping_ = json::parse(file).get<std::map<std::string, std::string>>();
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR << "Failed to load filename mapping: " << e.what();
    }
}

void FilenameMap::saveInternal() {
    try {
        std::ofstream file(mappingFile_);
        file << json(mapping_).dump(2);     // 缩进2空格
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to save filename mapping: " << e.what();
    }
}