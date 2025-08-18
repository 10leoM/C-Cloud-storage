#include "FileDownContext.h"
#include "Logger.h"

FileDownContext::FileDownContext(const std::string &filepath, const std::string &originalFilename)
    : filepath_(filepath), originalFilename_(originalFilename), fileSize_(0), currentPosition_(0), isComplete_(false)
{
    fileSize_ = fs::file_size(filepath_);
    file_.open(filepath_, std::ios::binary | std::ios::in);
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open file: " + filepath_);
    }
    LOG_INFO << "Opening file for download: " << filepath_ << ", size: " << fileSize_;
}

FileDownContext::~FileDownContext()
{
    if (file_.is_open()) {
        file_.close();
    }
}

void FileDownContext::seekTo(uintmax_t position)
{
    if(file_.is_open()) {
        file_.seekg(position);
        currentPosition_ = position;
        isComplete_ = false;
    }
    else{
        throw std::runtime_error("File is not open: " + filepath_);
    }
}

bool FileDownContext::readNextChunk(std::string &chunk)
{
    if(!file_.is_open() || isComplete_) {
        return false;
    }

    const uintmax_t chunkSize = 1024 * 1024; // 1MB
    uintmax_t remainingBytes = fileSize_ - currentPosition_;
    uintmax_t bytesToRead = std::min(chunkSize, remainingBytes);

    if (bytesToRead == 0) {
        isComplete_ = true;
        return false;
    }

    std::vector<char> buffer(bytesToRead);
    file_.read(buffer.data(), bytesToRead);
    chunk.assign(buffer.data(), bytesToRead);
    currentPosition_ += bytesToRead;

    LOG_INFO << "Read chunk of " << bytesToRead << " bytes, current position: "
             << currentPosition_ << "/" << fileSize_;
    return true;
}