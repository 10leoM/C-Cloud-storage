#pragma once

#include <string>
#include <memory>
#include <fstream>
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

// 文件上传上下文
class FileUploadContext
{
public:
    enum class State
    {
        kExpectHeaders,  // 等待头部
        kExpectContent,  // 等待文件内容
        kExpectBoundary, // 等待边界
        kComplete        // 上传完成
    };

    FileUploadContext(const std::string &filename, const std::string &originalFilename);
    ~FileUploadContext();

    void writeData(const char *data, size_t len);

    uintmax_t getTotalBytes() const { return totalbytes_; }
    const std::string &getFilename() const { return filename_; }
    const std::string &getOriginalFilename() const { return originalFilename_; }

    void setBoundary(const std::string &boundary) { boundary_ = boundary; }
    const std::string &getBoundary() const { return boundary_; }
    State getState() const { return state_; }
    void setState(State state) { state_ = state; }

private:
    std::string filename_;         // 保存在服务器上的文件名
    std::string originalFilename_; // 原始文件名
    std::ofstream file_;           // 文件流对象
    uintmax_t totalbytes_;         // 已写入的总字节数
    State state_;                  // 当前状态
    std::string boundary_;         // multipart边界
};