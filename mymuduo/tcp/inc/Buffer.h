#pragma once
#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include "Macro.h"

static const size_t PrePendIndex = 16;  // 预留区，前置长度/协议头
static const size_t InitialSize = 1024; // 初始容量

class Buffer
{
public:
    DISALLOW_COPY_AND_MOVE(Buffer);
    Buffer();  // 构造函数s
    ~Buffer(); // 析构函数

    char *begin();             // 获取缓冲区起始位置
    const char *begin() const; // const对象的begin函数

    char *beginread();             // 获取可读数据的起始位置
    const char *beginread() const; // const对象的可读数据起始位置

    char *beginwrite();             // 获取可写数据的起始位置
    const char *beginwrite() const; // const对象的可写数据起始位置

    void Append(const char *message);             // 追加C串（遇'\0'截断）
    void Append(const char *message, size_t len); // 追加定长（仍在首个'\0'截断）
    void Append(const std::string &message);      // 追加 std::string
    void Append(const void *data, size_t len);    // 追加原始二进制

    // 获取可读、可写和预留空间的大小
    size_t GetReadablebytes() const;    // [read, write)
    size_t GetWritablebytes() const;    // [write, capacity)
    size_t GetPrependablebytes() const; // [0, read)

    // 查看数据，但不更新读取索引
    char *Peek();             // 查看可读数据
    const char *Peek() const; // const对象的查看可读数据
    std::string PeekAsString(size_t len);
    std::string PeekAllAsString();

    // 取数据，取出后更新读取索引
    void Retrieve(size_t len);
    std::string RetrieveAsString(size_t len);

    // 全部取完
    void RetrieveAll();
    std::string RetrieveAllAsString();

    // 某个索引之前取数据
    void RetrieveUtil(const char *end);
    std::string RetrieveUtilAsString(const char *end);

    // 查看空间
    void EnsureWritableBytes(size_t len);

    // 查找工具
    const char *findCRLF() const;
    const char *findCRLF(const char *start) const;
    const char *findEOL() const;

    // 前置写入
    void Prepend(const void *data, size_t len);
    void PrependInt8(int8_t x);
    void PrependInt16(int16_t x);
    void PrependInt32(int32_t x);

    // readv 读取 fd
    ssize_t readFd(int fd, int *savedErrno);

    void toUpper();



private:
    std::vector<char> buf_;
    size_t read_index_;
    size_t write_index_;
    void makeSpace(size_t len); // 确保有足够的空间写入数据
};