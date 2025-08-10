#pragma once
#include <functional>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <future>
#include <iostream>
#include <condition_variable>
#include "Macro.h"

class ThreadPool
{
private:
    std::vector<std::thread>threads;                // 线程池中的线程
    std::queue<std::function<void()>> tasks;        // 任务队列
    std::mutex tasks_mtx;                           // 任务队列的互斥锁
    std::condition_variable cv;                     // 条件变量，用于线程间的同步
    bool stop;                                      // 停止标志，指示线程池是否应该停止
    
public:
    DISALLOW_COPY_AND_MOVE(ThreadPool);
    ThreadPool(int size = 10);                      // 构造函数，创建指定数量
    ~ThreadPool();                                  // 析构函数，清理资源
    template<class F, class... Args>
    auto add(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>;
};

//不能放在cpp文件，原因是C++编译器不支持模版的分离编译
template<class F, class... Args>
auto ThreadPool::add(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(tasks_mtx);

        // don't allow enqueueing after stopping the pool
        if(stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        tasks.emplace([task](){ (*task)(); });
    }
    cv.notify_one();
    return res;
}
