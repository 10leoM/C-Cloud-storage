#ifndef MYMUDUO_BASE_THREADPOOL_H
#define MYMUDUO_BASE_THREADPOOL_H

#include "Condition.h"
#include "Mutex.h"
#include "Thread.h"
#include "Types.h"

#include <deque>
#include <vector>
#include <memory>
#include <functional>

namespace mymuduo
{

/**
 * @brief 线程池类
 * 
 * 特点：
 * 1. 固定数量线程
 * 2. 任务队列
 * 3. 支持优雅关闭
 * 4. 支持自定义任务类型
 */
class ThreadPool : noncopyable
{
public:
    // 任务函数类型
    using Task = std::function<void()>;

    /**
     * @brief 构造函数
     * @param nameArg 线程池名称
     * @param maxSize 最大线程数
     */
    explicit ThreadPool(const string& nameArg = string("ThreadPool"));
    ~ThreadPool();

    /**
     * @brief 设置线程池大小并启动
     * @param numThreads 线程数量
     */
    void setThreadSize(int numThreads) { threadSize_ = numThreads; }

    /**
     * @brief 设置最大队列大小
     * @param maxSize 最大队列大小
     */
    void setMaxQueueSize(size_t maxSize) { maxQueueSize_ = maxSize; }

    /**
     * @brief 启动线程池
     */
    void start(int numThreads = 4);

    /**
     * @brief 关闭线程池
     */
    void stop();

    /**
     * @brief 添加任务到线程池
     * @param task 任务函数
     */
    void run(Task task);

    /**
     * @brief 获取任务队列大小
     */
    size_t queueSize() const;

private:
    /**
     * @brief 获取一个待执行的任务
     * @return Task 任务函数
     */
    Task take();

    /**
     * @brief 线程执行函数
     */
    void runInThread();

    /**
     * @brief 检查任务队列是否已满
     */
    bool isFull() const;

    /**
     * @brief 线程池名称
     */
    const string name_;

    /**
     * @brief 互斥锁
     */
    mutable MutexLock mutex_;

    /**
     * @brief 条件变量，用于任务队列非空条件
     */
    Condition notEmpty_;

    /**
     * @brief 条件变量，用于任务队列非满条件
     */
    Condition notFull_;

    /**
     * @brief 线程池是否运行标志
     */
    bool running_;

    /**
     * @brief 最大任务队列大小
     */
    size_t maxQueueSize_;

    /**
     * @brief 线程池大小
     */
    int threadSize_;

    /**
     * @brief 任务队列
     */
    std::deque<Task> queue_;

    /**
     * @brief 线程列表
     */
    std::vector<std::unique_ptr<Thread>> threads_;
};

} // namespace mymuduo

#endif // MYMUDUO_BASE_THREADPOOL_H 