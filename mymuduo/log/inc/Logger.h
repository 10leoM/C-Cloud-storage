#pragma once

#include <string>
#include <thread>
#include "Macro.h"
#include "TimeStamp.h"
#include "LogStream.h"

class Logger
{
public:
    enum LogLevel
    {
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL
    };

    class SourceFile // 用于存储源文件信息
    {
    public:
        SourceFile(const char *data);
        const char *data_;
        int size_;
    };

    DISALLOW_COPY_AND_MOVE(Logger);
    // 构造函数，主要是用于构造Impl
    Logger(const char *file_, int line, LogLevel level = INFO);
    // Logger(const char *file_, int line, LogLevel level, const char *func);
    ~Logger();

    // 用于日志宏，返回Impl的输出流
    LogStream &GetStream();

    // 全局方法，设置日志全局日志级别，flush输出目的地
    static LogLevel GetLogLevel();
    static void SetLogLevel(LogLevel level);

    typedef void (*OutputFunc)(const char *data, int len); // 定义函数指针
    typedef void (*FlushFunc)();
    // 默认fwrite到stdout
    static void SetOutput(OutputFunc);
    // 默认fflush到stdout
    static void SetFlush(FlushFunc);

private:
    // 私有类，对日志信息进行组装，不对外开放
    class Impl
    {
    public:
        DISALLOW_COPY_AND_MOVE(Impl);

        Impl(Logger::LogLevel level, const SourceFile &source, int line);
        void FormattedTime(); // 格式化时间信息
        void Finish();        // 完成格式化，并补充输出源码文件和源码位置

        LogStream &GetStream();
        const char *GetLoglevel() const; // 获取LogLevel的字符串
        Logger::LogLevel level_;         // 日志级别

    private:
        Logger::SourceFile sourcefile_; // 源代码名称
        int line_;                      // 源代码行数
        LogStream stream_;              // 日志缓存流
    };
    Impl impl_;
};

extern Logger::Logger::LogLevel g_logLevel; // 全局日志级别
inline Logger::LogLevel Logger::GetLogLevel()
{
    return g_logLevel;
}

// 日志宏
#define LOG_DEBUG                               \
    if (Logger::GetLogLevel() <= Logger::DEBUG) \
    Logger(__FILE__, __LINE__, Logger::DEBUG, __func__).GetStream()
#define LOG_INFO                               \
    if (Logger::GetLogLevel() <= Logger::INFO) \
    Logger(__FILE__, __LINE__, Logger::INFO).GetStream()
#define LOG_WARN Logger(__FILE__, __LINE__, Logger::WARN).GetStream()
#define LOG_ERROR Logger(__FILE__, __LINE__, Logger::ERROR).GetStream()
#define LOG_FATAL Logger(__FILE__, __LINE__, Logger::FATAL).GetStream()
