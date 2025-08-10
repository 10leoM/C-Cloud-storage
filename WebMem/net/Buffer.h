#ifndef MYMUDUO_NET_BUFFER_H
#define MYMUDUO_NET_BUFFER_H

#include <algorithm>
#include <vector>
#include <string>
#include <assert.h>
#include <cstring>

namespace mymuduo {
namespace net {

/// @brief 网络库底层的缓冲区类
/// 
/// Buffer类提供了一个连续的内存缓冲区，用于网络读写操作。
/// 缓冲区被分为4个区域：
/// +-------------------+------------------+------------------+
/// |    prependable   |     readable     |     writable     |
/// +-------------------+------------------+------------------+
/// |                  |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=    size
///
/// 其中：
/// - prependable区域用于在数据前添加额外信息（如消息长度）
/// - readable区域存储可读数据
/// - writable区域用于写入新数据
class Buffer {
public:
    static const size_t kCheapPrepend = 8;     // 预留空间大小
    static const size_t kInitialSize = 1024;   // 初始缓冲区大小

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize),
          readerIndex_(kCheapPrepend),
          writerIndex_(kCheapPrepend)
    {
        assert(readableBytes() == 0);
        assert(writableBytes() == initialSize);
        assert(prependableBytes() == kCheapPrepend);
    }

    // 默认拷贝构造和赋值是安全的
    // Buffer对象的swap是O(1)复杂度的
    void swap(Buffer& rhs) {
        buffer_.swap(rhs.buffer_);
        std::swap(readerIndex_, rhs.readerIndex_);
        std::swap(writerIndex_, rhs.writerIndex_);
    }

    /// @brief 可读字节数
    size_t readableBytes() const { return writerIndex_ - readerIndex_; }

    /// @brief 可写字节数
    size_t writableBytes() const { return buffer_.size() - writerIndex_; }

    /// @brief 可前置字节数
    size_t prependableBytes() const { return readerIndex_; }

    /// @brief 返回可读数据的起始位置
    const char* peek() const { return begin() + readerIndex_; }

    /// @brief 查找CRLF
    const char* findCRLF() const {
        const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF+2);
        return crlf == beginWrite() ? nullptr : crlf;
    }

    /// @brief 查找CRLF从start位置开始
    const char* findCRLF(const char* start) const {
        assert(peek() <= start);
        assert(start <= beginWrite());
        const char* crlf = std::search(start, beginWrite(), kCRLF, kCRLF+2);
        return crlf == beginWrite() ? nullptr : crlf;
    }

    /// @brief 查找EOL
    const char* findEOL() const {
        const void* eol = memchr(peek(), '\n', readableBytes());
        return static_cast<const char*>(eol);
    }

    /// @brief 查找EOL从start位置开始
    const char* findEOL(const char* start) const {
        assert(peek() <= start);
        assert(start <= beginWrite());
        const void* eol = memchr(start, '\n', beginWrite() - start);
        return static_cast<const char*>(eol);
    }

    /// @brief 读取len个字节
    void retrieve(size_t len) {
        assert(len <= readableBytes());
        if (len < readableBytes()) {
            readerIndex_ += len;
        } else {
            retrieveAll();
        }
    }

    /// @brief 读取直到end位置
    void retrieveUntil(const char* end) {
        assert(peek() <= end);
        assert(end <= beginWrite());
        retrieve(end - peek());
    }

    /// @brief 读取所有数据
    void retrieveAll() {
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    /// @brief 读取所有数据并转换为string
    std::string retrieveAllAsString() {
        return retrieveAsString(readableBytes());
    }

    /// @brief 读取len个字节并转换为string
    std::string retrieveAsString(size_t len) {
        assert(len <= readableBytes());
        std::string result(peek(), len);
        retrieve(len);
        return result;
    }

    /// @brief 确保有足够的可写空间
    void ensureWritableBytes(size_t len) {
        if (writableBytes() < len) {
            makeSpace(len);
        }
        assert(writableBytes() >= len);
    }

    /// @brief 追加数据
    void append(const char* data, size_t len) {
        ensureWritableBytes(len);
        std::copy(data, data+len, beginWrite());
        hasWritten(len);
    }

    /// @brief 追加string
    void append(const std::string& str) {
        append(str.data(), str.size());
    }

    /// @brief 返回可写位置
    char* beginWrite() { return begin() + writerIndex_; }
    const char* beginWrite() const { return begin() + writerIndex_; }

    /// @brief 已写入len个字节
    void hasWritten(size_t len) {
        assert(len <= writableBytes());
        writerIndex_ += len;
    }

    /// @brief 回退写位置
    void unwrite(size_t len) {
        assert(len <= readableBytes());
        writerIndex_ -= len;
    }

    /// @brief 前置数据
    void prepend(const void* data, size_t len) {
        assert(len <= prependableBytes());
        readerIndex_ -= len;
        const char* d = static_cast<const char*>(data);
        std::copy(d, d+len, begin()+readerIndex_);
    }

    /// @brief 收缩空间
    void shrink(size_t reserve) {
        Buffer other;
        other.ensureWritableBytes(readableBytes()+reserve);
        other.append(peek(), readableBytes());
        swap(other);
    }

    /// @brief 缓冲区容量
    size_t capacity() const { return buffer_.capacity(); }

    /// @brief 从fd读取数据
    ssize_t readFd(int fd, int* savedErrno);

private:
    char* begin() { return &*buffer_.begin(); }
    const char* begin() const { return &*buffer_.begin(); }

    void makeSpace(size_t len) {
        if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
            buffer_.resize(writerIndex_+len);
        } else {
            assert(kCheapPrepend < readerIndex_);
            size_t readable = readableBytes();
            std::copy(begin()+readerIndex_,
                     begin()+writerIndex_,
                     begin()+kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
            assert(readable == readableBytes());
        }
    }

private:
    std::vector<char> buffer_;  // 缓冲区
    size_t readerIndex_;        // 读位置
    size_t writerIndex_;        // 写位置

    static const char kCRLF[];  // CRLF字符
};

}  // namespace net
}  // namespace mymuduo

#endif  // MYMUDUO_NET_BUFFER_H 