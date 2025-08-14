#pragma once

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include "Macro.h"
#include "Logger.h"
#include "Latch.h"

static const double BufferWriteTimeout = 3.0;              // 等待写入的时间
static const int64_t FileMaximumSize = 1024 * 1024 * 1024; // 单个文件最大的容量

class AsyncLogging
{
public:
    typedef FixedBuffer<FixedLargeBuffferSize> Buffer;

    AsyncLogging(const char *filepath = nullptr);
    ~AsyncLogging();

    void Start(); // 启动异步日志
    void Stop();  // 停止异步日志

    void Append(const char *data, int len); // 向日志追加数据
    void Flush();                           // 刷新日志
    void ThreadFunc();                      // 后台线程函数

private:
    bool running_;               // 异步日志是否正在运行
    const char *filepath_;       // 日志文件路径
    std::mutex mutex_;           // 互斥锁
    std::condition_variable cv_; // 条件变量
    Latch latch_;                // 用于线程同步的计数器
    std::thread thread_;         // 后台线程

    std::unique_ptr<Buffer> current_;              // 当前的缓冲区
    std::unique_ptr<Buffer> next_;                 // 空闲的缓冲区
    std::vector<std::unique_ptr<Buffer>> buffers_; // 已满的缓冲区列表

};