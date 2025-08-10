#pragma once
#include <string>
#include <vector>
#include "Macro.h"

// class Buffer
// {
// private:
//     std::string buf; // 内部缓冲区，使用std::string存储数据

// public:
//     DISALLOW_COPY_AND_MOVE(Buffer);
//     Buffer();  // 构造函数
//     ~Buffer(); // 析构函数

//     void append(const char *_str, int _size); // 向缓冲区追加数据
//     ssize_t size();                           // 获取缓冲区大小
//     const char *c_str();                      // 获取缓冲区内容的C风格字符串
//     void clear();                             // 清空缓冲区
//     void getline();                           // 从标准输入读取一行数据到缓冲区
//     void toUpper();                           // 将缓冲区内容转换为大写
//     void setBuf(const char *_str);            // 设置缓冲区内容
// };

static const int PrePendIndex = 8;  // prependindex长度
static const int InitialSize = 1024; // 初始化开辟空间长度

class Buffer
{
public:
    DISALLOW_COPY_AND_MOVE(Buffer);
    Buffer();  // 构造函数
    ~Buffer(); // 析构函数

    char *begin();             // 获取缓冲区起始位置
    const char *begin() const; // const对象的begin函数

    char *beginread();             // 获取可读数据的起始位置
    const char *beginread() const; // const对象的可读数据起始位置

    char *beginwrite();             // 获取可写数据的起始位置
    const char *beginwrite() const; // const对象的可写数据起始位置

    void Append(const char *message);          // 向缓冲区追加数据
    void Append(const char *message, int len); // 向缓冲区追加指定
    void Append(const std::string &message);   // 向缓冲区追加字符串

    // 获取可读、可写和预留空间的大小
    int GetReadablebytes() const;    // 可读字节数
    int GetWritablebytes() const;    // 可写字节数
    int GetPrependablebytes() const; // 预留字节数

    // 查看数据，但不更新读取索引
    char *Peek();             // 查看可读数据
    const char *Peek() const; // const对象的查看可读数据
    std::string PeekAsString(int len);
    std::string PeekAllAsString();

    // 取数据，取出后更新读取索引
    void Retrieve(int len);
    std::string RetrieveAsString(int len);

    // 全部取完
    void RetrieveAll();
    std::string RetrieveAllAsString();

    // 某个索引之前取数据
    void RetrieveUtil(const char *end);
    std::string RetrieveUtilAsString(const char *end);

    // 查看空间
    void EnsureWritableBytes(int len);

    void toUpper();

private:
    std::vector<char> buf_; // 内部缓冲区，使用std::vector存储数据
    int read_index_;        // 读取索引
    int write_index_;       // 写入索引
};