#ifndef MYMUDUO_BASE_LOGGING_H
#define MYMUDUO_BASE_LOGGING_H

#include "LogStream.h"
#include "Timestamp.h"

namespace mymuduo
{

class Logger
{
public:
    enum LogLevel
    {
        TRACE,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL,
        NUM_LOG_LEVELS,
    };

    // 编译时计算源文件名
    class SourceFile
    {
    public:
        template<int N>
        SourceFile(const char (&arr)[N])
            : data_(arr),
              size_(N-1)
        {
            const char* slash = strrchr(data_, '/');
            if (slash)
            {
                data_ = slash + 1;
                size_ -= static_cast<int>(data_ - arr);
            }
        }

        explicit SourceFile(const char* filename)
            : data_(filename)
        {
            const char* slash = strrchr(filename, '/');
            if (slash)
            {
                data_ = slash + 1;
            }
            size_ = static_cast<int>(strlen(data_));
        }

        const char* data_;
        int size_;
    };

    Logger(SourceFile file, int line);
    Logger(SourceFile file, int line, LogLevel level);
    Logger(SourceFile file, int line, LogLevel level, const char* func);
    Logger(SourceFile file, int line, bool toAbort);
    ~Logger();

    LogStream& stream() { return impl_.stream_; }

    static LogLevel logLevel();
    static void setLogLevel(LogLevel level);

    typedef void (*OutputFunc)(const char* msg, int len);
    typedef void (*FlushFunc)();
    static void setOutput(OutputFunc);
    static void setFlush(FlushFunc);

private:
    class Impl
    {
    public:
        typedef Logger::LogLevel LogLevel;
        Impl(LogLevel level, int old_errno, const SourceFile& file, int line);
        void formatTime();
        void finish();

        Timestamp time_;
        LogStream stream_;
        LogLevel level_;
        int line_;
        SourceFile basename_;
    };

    Impl impl_;
};

extern Logger::LogLevel g_logLevel;

inline Logger::LogLevel Logger::logLevel()
{
    return g_logLevel;
}

#define LOG_TRACE if (mymuduo::Logger::logLevel() <= mymuduo::Logger::TRACE) \
    mymuduo::Logger(__FILE__, __LINE__, mymuduo::Logger::TRACE, __func__).stream()
#define LOG_DEBUG if (mymuduo::Logger::logLevel() <= mymuduo::Logger::DEBUG) \
    mymuduo::Logger(__FILE__, __LINE__, mymuduo::Logger::DEBUG, __func__).stream()
#define LOG_INFO if (mymuduo::Logger::logLevel() <= mymuduo::Logger::INFO) \
    mymuduo::Logger(__FILE__, __LINE__).stream()
#define LOG_WARN mymuduo::Logger(__FILE__, __LINE__, mymuduo::Logger::WARN).stream()
#define LOG_ERROR mymuduo::Logger(__FILE__, __LINE__, mymuduo::Logger::ERROR).stream()
#define LOG_FATAL mymuduo::Logger(__FILE__, __LINE__, mymuduo::Logger::FATAL).stream()
#define LOG_SYSERR mymuduo::Logger(__FILE__, __LINE__, false).stream()
#define LOG_SYSFATAL mymuduo::Logger(__FILE__, __LINE__, true).stream()

const char* strerror_tl(int savedErrno);

}  // namespace mymuduo

#endif  // MYMUDUO_BASE_LOGGING_H 