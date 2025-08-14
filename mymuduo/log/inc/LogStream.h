#pragma once

#include <string>
#include <algorithm>
#include <cassert>
#include <cstring>
#include "Macro.h"

const int FixedBufferSize = 4096;              // 缓冲区大小
const int FixedLargeBuffferSize = 4096 * 1000; // 大缓冲区大小

// 修改为模板类
template <int SIZE>
class FixedBuffer
{
public:
    FixedBuffer();
    ~FixedBuffer();

    void append(const char *buf, int len); // 向缓冲区追加数据
    const char *GetData() const;           // 获取缓冲区数据
    int GetLen() const;                    // 获取当前长度

    char *Getcur();       // 获取当前指针
    int GetAvail() const; // 获取剩余可用空间
    void AddLen(int len); // 增加长度

    void reset();               // 重置缓冲区
    const char *GetEnd() const; // 获取缓冲区末端地址
    void clear();               // 清空缓冲区
    void bzero();               // 清空缓冲区,置数为0

private:
    char data_[SIZE]; // 缓冲区数据
    char *cur_;       // 当前指针
};

class LogStream // 日志输出流类
{
public:
    DISALLOW_COPY_AND_MOVE(LogStream);
    LogStream();
    ~LogStream();

    void append(const char *data, int len);                // 向缓冲区追加数据
    const FixedBuffer<FixedBufferSize> &GetBuffer() const; // 获取缓冲区引用
    void resetBuffer();                                    // 重置缓冲区

    LogStream &operator<<(bool v);
    // 整形数据的字符串转换、保存到缓冲区； 内部均调用 void formatInteger(T); 函数
    LogStream &operator<<(short num);
    LogStream &operator<<(unsigned short num);
    LogStream &operator<<(int num);
    LogStream &operator<<(unsigned int num);
    LogStream &operator<<(long num);
    LogStream &operator<<(unsigned long num);
    LogStream &operator<<(long long num);
    LogStream &operator<<(unsigned long long num);

    // 浮点类型数据转换成字符串
    LogStream &operator<<(const float &num);
    LogStream &operator<<(const double &num);

    // 原生字符串输出到缓冲区
    LogStream &operator<<(char v);
    LogStream &operator<<(const char *str);

    // 标准字符串std::string输出到缓冲区
    LogStream &operator<<(const std::string &v);

private:
    template <typename T>
    void formatInteger(T value);          // 整数格式化函数
    FixedBuffer<FixedBufferSize> buffer_; // 缓冲区对象
};

template <typename T>
void LogStream::formatInteger(T value)
{
    if (buffer_.GetAvail() < 32)
        return;
    char *buf = buffer_.Getcur();
    char *now = buf;

    do
    {
        *now++ = '0' + value % 10;
        value /= 10;
    } while (value != 0);
    if (value < 0)
    {
        *now++ = '-';
    }
    std::reverse(buf, now);    // 反转字符串
    buffer_.AddLen(now - buf); // 更新缓冲区长度
}

class Fmt // 格式化类，用于格式化字符串
{
public:
    template <typename T>
    Fmt(const char *fmt, T val);

    const char *data() const { return buf_; }
    int length() const { return length_; }

private:
    char buf_[32];
    int length_;
};

template <typename T>
Fmt::Fmt(const char *fmt, T val)
{
    static_assert(std::is_arithmetic<T>::value == true, "Must be arithmetic type"); // 确保T是算术类型
    length_ = snprintf(buf_, sizeof(buf_), fmt, val);                               // 格式化字符串
    assert(static_cast<size_t>(length_) < sizeof(buf_));                            // 格式化后的字符串长度不能超过缓冲区大小
}

inline LogStream &operator<<(LogStream &s, const Fmt &fmt)
{
    s.append(fmt.data(), fmt.length());
    return s;
}

// 显式实例化模板，避免重复编译
template Fmt::Fmt(const char *fmt, char);

template Fmt::Fmt(const char *fmt, short);
template Fmt::Fmt(const char *fmt, unsigned short);
template Fmt::Fmt(const char *fmt, int);
template Fmt::Fmt(const char *fmt, unsigned int);
template Fmt::Fmt(const char *fmt, long);
template Fmt::Fmt(const char *fmt, unsigned long);
template Fmt::Fmt(const char *fmt, long long);
template Fmt::Fmt(const char *fmt, unsigned long long);

template Fmt::Fmt(const char *fmt, float);


// FixedBuffer 实现
template <int SIZE>
FixedBuffer<SIZE>::FixedBuffer() : cur_(data_) {}
template <int SIZE>
FixedBuffer<SIZE>::~FixedBuffer() {}

template <int SIZE>
void FixedBuffer<SIZE>::append(const char *buf, int len) 
{
    if (GetAvail() > len) 
    {
        memcpy(cur_, buf, len);
        cur_ += len;
    }
}
template <int SIZE>
const char *FixedBuffer<SIZE>::GetData() const { return data_; }
template <int SIZE>
int FixedBuffer<SIZE>::GetLen() const { return static_cast<int>(cur_ - data_); }
template <int SIZE>
char *FixedBuffer<SIZE>::Getcur() { return cur_; }
template <int SIZE>
int FixedBuffer<SIZE>::GetAvail() const { return static_cast<int>(GetEnd() - cur_); }
template <int SIZE>
void FixedBuffer<SIZE>::AddLen(int len) { cur_ += len; }
template <int SIZE>
void FixedBuffer<SIZE>::reset() { clear(); }
template <int SIZE>
const char *FixedBuffer<SIZE>::GetEnd() const { return data_ + sizeof(data_); }
template <int SIZE>
void FixedBuffer<SIZE>::clear() { bzero();}
template <int SIZE>
void FixedBuffer<SIZE>::bzero() { memset(data_, 0, sizeof(data_)); cur_ = data_; }