#pragma once

#include <string>
#include <memory>
#include <fstream>
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

// 文件下载上下文
class FileDownContext
{
public:
    FileDownContext(const std::string &filepath, const std::string &originalFilename);
    ~FileDownContext();

    void seekTo(uintmax_t position);        //  定位到文件的指定位置
    bool readNextChunk(std::string &chunk); //  读取下一个数据块

    bool isComplete() const { return isComplete_; }
    uintmax_t getCurrentPosition() const { return currentPosition_; }
    uintmax_t getFileSize() const { return fileSize_; }
    const std::string &getOriginalFilename() const { return originalFilename_; }

private:
    std::string filepath_;         // 文件路径
    std::string originalFilename_; // 原始文件名
    std::ifstream file_;           // 文件流
    uintmax_t fileSize_;           // 文件总大小
    uintmax_t currentPosition_;    // 当前读取位置
    bool isComplete_;              // 是否完成
};