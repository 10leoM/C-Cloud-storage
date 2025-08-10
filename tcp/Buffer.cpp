#include "Buffer.h"
#include <iostream>
#include <cstring>
#include <assert.h>

// Buffer::Buffer()
// {
// }

// Buffer::~Buffer()
// {
// }


// void Buffer::append(const char* _str, int _size){
//     for(int i = 0; i < _size; ++i){
//         if(_str[i] == '\0') break;
//         buf.push_back(_str[i]);
//     }
// }

// ssize_t Buffer::size(){
//     return buf.size();
// }

// const char* Buffer::c_str(){
//     return buf.c_str();
// }

// void Buffer::clear(){
//     buf.clear();
// }

// void Buffer::getline(){
//     buf.clear();
//     std::getline(std::cin, buf);
// }

// void Buffer::toUpper() {
//     for (size_t i = 0; i < buf.size(); ++i) {
//         buf[i] = toupper(buf[i]);
//     }
// }

// void Buffer::setBuf(const char* _str) {
//     buf.clear();
//     append(_str, strlen(_str));
// }

Buffer::Buffer() : buf_(InitialSize), read_index_(PrePendIndex), write_index_(PrePendIndex) {}

Buffer::~Buffer() {}

char *Buffer::begin() { return &*buf_.begin(); }
const char *Buffer::begin() const { return &*buf_.begin(); }
char* Buffer::beginread() { return begin() + read_index_; } 
const char* Buffer::beginread() const { return begin() + read_index_; }
char* Buffer::beginwrite() { return begin() + write_index_; }
const char* Buffer::beginwrite() const { return begin() + write_index_; }

void Buffer::Append(const char* message) {
    Append(message, static_cast<int>(strlen(message)));
}
void Buffer::Append(const char* message, int len) {
    for(int i = 0; i < len; ++i) {
        if(message[i] == '\0')
        {
            len = i; // 如果遇到'\0'，则只追加到此处
            break;
        }
    }
    EnsureWritableBytes(len);
    std::copy(message, message + len, beginwrite());
    write_index_ += len;
}
void Buffer::Append(const std::string& message) {
    Append(message.data(), static_cast<int>(message.size()));
}

int Buffer::GetReadablebytes() const { return write_index_ - read_index_; }
int Buffer::GetWritablebytes() const { return static_cast<int>(buf_.size()) - write_index_; }
int Buffer::GetPrependablebytes() const { return read_index_; }

char *Buffer::Peek() { return beginread(); }
const char *Buffer::Peek() const { return beginread(); }
std::string Buffer::PeekAsString(int len) {
    return std::string(beginread(), beginread() + len);
}
std::string Buffer::PeekAllAsString() {
    return std::string(beginread(), beginwrite());
}

void Buffer::Retrieve(int len) {
    assert(GetReadablebytes() > len);
    if (len + read_index_ < write_index_) {
        read_index_ += len; // 更新读取索引
    } else {
        RetrieveAll(); // 读完全部数据
    }
}
void Buffer::RetrieveAll() {
    write_index_ = PrePendIndex; // 重置写入索引
    read_index_ = write_index_;  // 重置读取索引
}
void Buffer::RetrieveUtil(const char *end) {
    assert(end >= beginread() && end <= beginwrite());
    Retrieve(static_cast<int>(end - beginread()));
}

std::string Buffer::RetrieveAsString(int len) {
    assert(GetReadablebytes() >= len);
    std::string result = std::move(PeekAsString(len));
    Retrieve(len);
    return result;
}
std::string Buffer::RetrieveAllAsString() {
    std::string result = std::move(PeekAllAsString());
    RetrieveAll();
    return result;
}
std::string Buffer::RetrieveUtilAsString(const char *end) {
    assert(end >= beginread() && end <= beginwrite());
    std::string result = std::move(PeekAsString(end - beginread()));
    Retrieve(end - beginread());
    return result;
}

void Buffer::EnsureWritableBytes(int len) {
    if(GetWritablebytes() >= len) 
        return;
    if(GetWritablebytes() + GetPrependablebytes() >= PrePendIndex + len) // 可写空间 + 可写空间前的空间 < 所需空间
    {
        std::copy(beginread(), beginwrite(), begin() + PrePendIndex);
        write_index_ = PrePendIndex + GetReadablebytes();
        read_index_ = PrePendIndex;
    }
    else    // 空间不足
    {
        buf_.resize(write_index_ + len);
    }
}

void Buffer::toUpper() {
    for (size_t i = read_index_; i < write_index_; ++i) {
        buf_[i] = toupper(buf_[i]);
    }
}