#ifndef MYMUDUO_NET_EVENTLOOPTHREADPOOL_H
#define MYMUDUO_NET_EVENTLOOPTHREADPOOL_H

#include "base/noncopyable.h"
#include "base/Types.h"

#include <functional>
#include <memory>
#include <vector>
#include <string>
#include <atomic>

namespace mymuduo {
namespace net {

class EventLoop;
class EventLoopThread;

/**
 * @brief 事件循环线程池类
 * 
 * 该类管理一组EventLoopThread，用于多线程事件处理。
 * 通常用于TCP服务器，其中mainLoop接受连接，
 * 而IO线程池中的线程负责处理已建立连接的IO事件。
 * 
 * 特点：
 * 1. 支持动态调整线程数量
 * 2. Round-robin方式分配连接
 * 3. 支持线程初始化回调
 * 4. 支持优雅关闭
 * 5. 线程安全的设计
 */
class EventLoopThreadPool : noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    /**
     * @brief 构造函数
     * @param baseLoop 主事件循环（通常是acceptor所属的loop）
     * @param nameArg 线程池名称
     */
    EventLoopThreadPool(EventLoop* baseLoop, const string& nameArg);
    
    /**
     * @brief 析构函数
     * 会等待所有线程安全退出
     */
    ~EventLoopThreadPool();

    /**
     * @brief 设置线程数量
     * @param numThreads 线程数量
     * 如果numThreads为0，所有IO都在baseLoop线程中进行
     */
    void setThreadNum(int numThreads) { numThreads_ = numThreads; }

    /**
     * @brief 启动线程池
     * @param cb 线程初始化回调函数
     */
    void start(const ThreadInitCallback& cb = ThreadInitCallback());

    /**
     * @brief 优雅地关闭线程池
     * 等待所有任务完成后再关闭
     */
    void stop();

    /**
     * @brief 获取下一个事件循环
     * 用于新连接的分发，采用round-robin方式分配
     */
    EventLoop* getNextLoop();

    /**
     * @brief 获取所有事件循环
     */
    std::vector<EventLoop*> getAllLoops();

    /**
     * @brief 是否已经启动
     */
    bool started() const { return started_; }

    /**
     * @brief 获取线程池名称
     */
    const string& name() const { return name_; }

    /**
     * @brief 获取当前线程数量
     */
    int numThreads() const { return numThreads_; }

    /**
     * @brief 获取当前活跃的事件循环数量
     */
    size_t size() const { return loops_.size(); }

private:
    EventLoop* baseLoop_;        // 主事件循环
    string name_;               // 线程池名称
    bool started_;             // 是否已启动
    int numThreads_;           // 线程数量
    std::atomic<int> next_;    // 下一个要被分配的线程索引
    std::vector<std::unique_ptr<EventLoopThread>> threads_;  // IO线程列表
    std::vector<EventLoop*> loops_;                          // EventLoop列表
};

} // namespace net
} // namespace mymuduo

#endif // MYMUDUO_NET_EVENTLOOPTHREADPOOL_H 