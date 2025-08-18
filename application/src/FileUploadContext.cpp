#include "FileUploadContext.h"
#include "Logger.h"

FileUploadContext::FileUploadContext(const std::string &filename, const std::string &originalFilename)
    : filename_(filename), originalFilename_(originalFilename), totalBytes_(0), state_(State::kExpectHeaders), boundary_("")
{
    // 确保目录存在
    fs::path filepath = fs::path(filename_).parent_path();
    if (!fs::exists(filepath))
    {
        fs::create_directories(filepath);
    }  
    // 打开文件流
    file_.open(filename_, std::ios::binary | std::ios::out);
    if (!file_.is_open())
    {
        LOG_ERROR<<"Failed to open file: " << filename_ << " for writing";
    }
}

FileUploadContext::~FileUploadContext()
{
    if(file_.is_open())
    {
        file_.close();
    }
}

void FileUploadContext::writeData(const char *data, size_t len)
{
    if(file_.is_open())
    {
        file_.write(data, len);
        file_.flush();
        totalBytes_ += len;
    }
}
