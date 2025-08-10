#ifndef MYMUDUO_BASE_LOGFILE_H
#define MYMUDUO_BASE_LOGFILE_H

#include "Mutex.h"
#include "Types.h"
#include <memory>

namespace mymuduo
{

namespace FileUtil
{
class AppendFile;
}

/**
 * @brief 日志文件类
 * 
 * 特点：
 * 1. 支持多线程安全的日志写入
 * 2. 支持按大小滚动
 * 3. 支持按时间刷新
 * 4. 支持自动压缩
 */
class LogFile : noncopyable
{
public:
    /**
     * @brief 构造函数
     * @param basename 日志文件基本名
     * @param rollSize 滚动大小，超过该大小创建新文件
     * @param threadSafe 是否线程安全
     * @param flushInterval 刷新间隔，秒
     * @param checkEveryN 检查是否需要滚动的频率
     */
    LogFile(const string& basename,
            off_t rollSize,
            bool threadSafe = true,
            int flushInterval = 3,
            int checkEveryN = 1024);
    ~LogFile();

    /**
     * @brief 追加日志
     * @param logline 日志内容
     * @param len 日志长度
     */
    void append(const char* logline, int len);

    /**
     * @brief 刷新到磁盘
     */
    void flush();

    /**
     * @brief 滚动日志文件
     */
    bool rollFile();

private:
    /**
     * @brief 无锁方式追加日志
     */
    void append_unlocked(const char* logline, int len);

    /**
     * @brief 获取日志文件名
     */
    static string getLogFileName(const string& basename, time_t* now);

    const string basename_;          // 日志文件基本名
    const off_t rollSize_;          // 滚动大小
    const int flushInterval_;       // 刷新间隔
    const int checkEveryN_;         // 检查频率

    int count_;                     // 计数器

    std::unique_ptr<MutexLock> mutex_;  // 互斥锁
    time_t startOfPeriod_;          // 开始记录的时间
    time_t lastRoll_;               // 上次滚动时间
    time_t lastFlush_;              // 上次刷新时间
    std::unique_ptr<FileUtil::AppendFile> file_;  // 文件对象

    const static int kRollPerSeconds_ = 60*60*24;  // 一天
};

} // namespace mymuduo

#endif // MYMUDUO_BASE_LOGFILE_H 