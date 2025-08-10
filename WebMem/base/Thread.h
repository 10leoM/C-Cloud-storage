#ifndef MYMUDUO_BASE_THREAD_H
#define MYMUDUO_BASE_THREAD_H

#include "Atomic.h"
#include "CountDownLatch.h"
#include "Types.h"

#include <functional>
#include <memory>
#include <pthread.h>

namespace mymuduo
{

/**
 * @brief 线程类，封装了pthread，提供RAII方式的线程管理
 * 
 * 特点：
 * 1. 使用std::function封装线程函数
 * 2. 使用RAII方式管理线程资源
 * 3. 提供线程计数功能
 * 4. 支持线程命名
 * 5. 使用CountDownLatch确保线程安全启动
 */
class Thread : noncopyable
{
public:
    // 线程回调函数类型
    typedef std::function<void ()> ThreadFunc;

    /**
     * @brief 构造函数
     * @param func 线程函数
     * @param name 线程名称，默认为空
     */
    explicit Thread(ThreadFunc func, const string& name = string());

    /**
     * @brief 析构函数，如果线程是joinable的，则join
     */
    ~Thread();

    /**
     * @brief 启动线程
     */
    void start();

    /**
     * @brief 等待线程结束
     */
    void join();

    /**
     * @brief 是否已启动
     */
    bool started() const { return started_; }

    /**
     * @brief 是否已加入
     */
    bool joined() const { return joined_; }

    /**
     * @brief 获取线程ID
     */
    pid_t tid() const { return tid_; }

    /**
     * @brief 获取线程名称
     */
    const string& name() const { return name_; }

    /**
     * @brief 获取当前活跃线程数
     */
    static int numCreated() { return numCreated_.get(); }

private:
    /**
     * @brief 设置默认线程名称
     */
    void setDefaultName();

    bool       started_;     // 是否已启动
    bool       joined_;      // 是否已join
    pthread_t  pthreadId_;   // pthread线程ID
    pid_t      tid_;        // 线程真实ID
    ThreadFunc func_;       // 线程函数
    string     name_;       // 线程名称
    CountDownLatch latch_;  // 用于确保线程安全启动

    static AtomicInt32 numCreated_; // 已创建的线程数
};

} // namespace mymuduo

#endif // MYMUDUO_BASE_THREAD_H 