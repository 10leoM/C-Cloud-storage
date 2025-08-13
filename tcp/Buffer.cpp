#include "Buffer.h"
#include <iostream>
#include <cstring>
#include <assert.h>
#include <algorithm>
#include <sys/uio.h>
#include <netinet/in.h>

Buffer::Buffer() : buf_(InitialSize), read_index_(PrePendIndex), write_index_(PrePendIndex) {}

Buffer::~Buffer() {}

char *Buffer::begin() { return &*buf_.begin(); }
const char *Buffer::begin() const { return &*buf_.begin(); }
char* Buffer::beginread() { return begin() + read_index_; } 
const char* Buffer::beginread() const { return begin() + read_index_; }
char* Buffer::beginwrite() { return begin() + write_index_; }
const char* Buffer::beginwrite() const { return begin() + write_index_; }

void Buffer::Append(const char* message) {
    Append(message, static_cast<size_t>(strlen(message)));
}
void Buffer::Append(const char* message, size_t len) {
    if(len) {
        EnsureWritableBytes(len);
        std::copy(message, message + len, beginwrite());
        write_index_ += len;
    }
}
void Buffer::Append(const std::string& message) {
    Append(message.data(), static_cast<size_t>(message.size()));
}
void Buffer::Append(const void *data, size_t len) {
    if(!len) return;
    EnsureWritableBytes(len);
    std::memcpy(beginwrite(), data, len);
    write_index_ += len;
}

size_t Buffer::GetReadablebytes() const { return write_index_ - read_index_; }
size_t Buffer::GetWritablebytes() const { return buf_.size() - write_index_; }
size_t Buffer::GetPrependablebytes() const { return read_index_; }

char *Buffer::Peek() { return beginread(); }
const char *Buffer::Peek() const { return beginread(); }
std::string Buffer::PeekAsString(size_t len) {
    assert(GetReadablebytes() >= len);
    return std::string(beginread(), beginread() + len);
}
std::string Buffer::PeekAllAsString() {
    return std::string(beginread(), beginwrite());
}

void Buffer::Retrieve(size_t len) {
    assert(GetReadablebytes() >= len);
    if (len < GetReadablebytes()) {
        read_index_ += len;
    } else {
        RetrieveAll();
    }
}
void Buffer::RetrieveAll() {
    write_index_ = PrePendIndex; // 重置写入索引
    read_index_ = write_index_;  // 重置读取索引
}
void Buffer::RetrieveUtil(const char *end) {
    assert(end >= beginread() && end <= beginwrite());
    Retrieve(static_cast<size_t>(end - beginread()));
}

std::string Buffer::RetrieveAsString(size_t len) {
    assert(GetReadablebytes() >= len);
    std::string result = PeekAsString(len);
    Retrieve(len);
    return result;
}
std::string Buffer::RetrieveAllAsString() {
    std::string result = PeekAllAsString();
    RetrieveAll();
    return result;
}
std::string Buffer::RetrieveUtilAsString(const char *end) {
    assert(end >= beginread() && end <= beginwrite());
    size_t len = static_cast<size_t>(end - beginread());
    std::string result = PeekAsString(len);
    Retrieve(len);
    return result;
}

void Buffer::EnsureWritableBytes(size_t len) {
    if (GetWritablebytes() >= len) return;
    makeSpace(len);
    assert(GetWritablebytes() >= len);
}

void Buffer::toUpper() {
    for (size_t i = read_index_; i < write_index_; ++i) {
        buf_[i] = toupper(buf_[i]);
    }
}

void Buffer::makeSpace(size_t len) {
    if (GetWritablebytes() + GetPrependablebytes() - PrePendIndex >= len) {
        size_t readable = GetReadablebytes();
        std::copy(beginread(), beginwrite(), begin() + PrePendIndex);
        read_index_ = PrePendIndex;
        write_index_ = read_index_ + readable;
    } else {
        buf_.resize(write_index_ + len);
    }
}

const char *Buffer::findCRLF() const { return findCRLF(beginread()); }
const char *Buffer::findCRLF(const char *start) const {
    static const char kCRLF[] = "\r\n";
    const char *res = std::search(start, beginwrite(), kCRLF, kCRLF + 2);
    return (res == beginwrite()) ? nullptr : res;
}
const char *Buffer::findEOL() const {
    const void *res = memchr(beginread(), '\n', GetReadablebytes());
    return static_cast<const char *>(res);
}
void Buffer::Prepend(const void *data, size_t len) {
    assert(len <= GetPrependablebytes());
    read_index_ -= len;
    std::memcpy(beginread(), data, len);
}
void Buffer::PrependInt8(int8_t x) { Prepend(&x, sizeof x); }
void Buffer::PrependInt16(int16_t x) {
    int16_t be = htons(static_cast<uint16_t>(x));
    Prepend(&be, sizeof be);
}
void Buffer::PrependInt32(int32_t x) {
    int32_t be = htonl(static_cast<uint32_t>(x));
    Prepend(&be, sizeof be);
}
ssize_t Buffer::readFd(int fd, int *savedErrno) {
    char extrabuf[65536];
    struct iovec vec[2];
    size_t writable = GetWritablebytes();
    vec[0].iov_base = beginwrite();
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;
    int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0) {
        if (savedErrno) *savedErrno = errno;
    } else if (static_cast<size_t>(n) <= writable) {
        write_index_ += static_cast<size_t>(n);
    } else {
        write_index_ += writable;
        size_t appendLen = static_cast<size_t>(n) - writable;
        Append(extrabuf, appendLen);                    // Append内部会检查是否有足够空间
    }
    return n;
}