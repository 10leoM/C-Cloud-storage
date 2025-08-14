#include "Logger.h"
#include "CurrentThread.h"
#include "TimeStamp.h"
#include <string.h>

// 为了实现多线程中日志时间格式化的效率，增加了两个__thread变量，
// 用于缓存当前线程存日期时间字符串、上一次日志记录的秒数
__thread char t_time[64];     // 当前线程的时间字符串 “年:月:日 时:分:秒”
__thread time_t t_lastsecond; // 当前线程上一次日志记录时的秒数

// 方便一个已知长度的字符串被送入buffer中
class Template
{
public:
    Template(const char *str, unsigned len) : str_(str), len_(len) {}
    const char *str_;
    const unsigned len_;
};
// 重载运算符，使LogStream可以处理Template类型的数据。
inline LogStream &operator<<(LogStream &s, Template v)
{
    s.append(v.str_, v.len_);
    return s;
}
// 重载运算符，使LogStream可以处理SourceFile类型的数据
inline LogStream &operator<<(LogStream &s, const Logger::SourceFile &v)
{
    s.append(v.data_, v.size_);
    return s;
}

// SourceFile类实现
Logger::SourceFile::SourceFile(const char *data) : data_(data), size_(static_cast<int>(strlen(data)))
{
    const char *forward_slash = strrchr(data, '/'); // 查找最后一个斜杠
    if (forward_slash)
    {
        data_ = forward_slash + 1;                 // 更新data_指向斜杠后的位置
        size_ -= static_cast<int>((data_ - data)); // 更新size_为新的长度
    }
}

// Impl类实现
Logger::Impl::Impl(Logger::LogLevel level, const Logger::SourceFile &source, int line) : level_(level), sourcefile_(source), line_(line)
{
    FormattedTime();      // 格式化时间
    CurrentThread::tid(); // 获取当前线程ID

    stream_ << Template(CurrentThread::tidString(), CurrentThread::tidStringLength());
    stream_ << Template(GetLoglevel(), 6); // 添加日志级别字符串
}

void Logger::Impl::FormattedTime()
{
    // 格式化输出时间
    TimeStamp now = TimeStamp::Now();
    time_t seconds = static_cast<time_t>(now.GetMicroseconds() / kMicrosecond2Second);
    int microseconds = static_cast<int>(now.GetMicroseconds() % kMicrosecond2Second);

    // 变更日志记录的时间，如果不在同一秒，则更新时间。

    if (t_lastsecond != seconds)
    {
        struct tm tm_time;
        localtime_r(&seconds, &tm_time);
        snprintf(t_time, sizeof(t_time), "%4d%02d%02d %02d:%02d:%02d.",
                 tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
                 tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
        t_lastsecond = seconds;
    }

    Fmt us(".%06dZ  ", microseconds);
    stream_ << Template(t_time, 17) << Template(us.data(), 9);
}

void Logger::Impl::Finish()
{
    stream_ << " - " << sourcefile_.data_ << ":" << line_ << "\n";
}

LogStream &Logger::Impl::GetStream() { return stream_; }

const char *Logger::Impl::GetLoglevel() const
{
    switch (level_)
    {
    case Logger::DEBUG:
        return "DEBUG";
    case Logger::INFO:
        return "INFO ";
    case Logger::WARN:
        return "WARN ";
    case Logger::ERROR:
        return "ERROR";
    case Logger::FATAL:
        return "FATAL";
    default:
        return "UNKNOWN";
    }
}

void defaultOutput(const char *data, int len) // 默认输出函数
{
    fwrite(data, 1, len, stdout);
}

void defaultFlush() // 默认刷新函数
{
    fflush(stdout);
}

Logger::OutputFunc outputFunc = defaultOutput;        // 全局输出函数指针
Logger::FlushFunc flushFunc = defaultFlush;           // 全局刷新函数指针
Logger::LogLevel g_logLevel = Logger::LogLevel::INFO; // 全局日志级别

// Logger类实现
Logger::Logger(const char *file_, int line, LogLevel level) : impl_(level, SourceFile(file_), line) {}
// Logger::Logger(const char *file_, int line, LogLevel level, const char *func) : impl_(level, SourceFile(file_), line, func) {}

Logger::~Logger()
{
    impl_.Finish();                                   // 完成日志格式化,补足源文件和行数
    const FixedBuffer<FixedBufferSize> &buf = GetStream().GetBuffer(); // 获取日志缓存
    outputFunc(buf.GetData(), buf.GetLen());          // 输出日志
    
    // 当日志级别为FATAL时，flush设备缓冲区并终止程序
    if (impl_.level_ == FATAL) {
        flushFunc();
        abort();
    }
}

LogStream &Logger::GetStream() { return impl_.GetStream(); }
void Logger::SetLogLevel(LogLevel level) { g_logLevel = level; }

void Logger::SetOutput(OutputFunc func)
{
    if (func)
    {
        outputFunc = func; // 设置自定义输出函数
    }
}

void Logger::SetFlush(FlushFunc func)
{
    if (func)
    {
        flushFunc = func; // 设置自定义刷新函数
    }
}