#include "ThreadPool.h"
#include <iostream>

ThreadPool::ThreadPool(int size):stop(false)
{
    threads.reserve(size);
    for(int i = 0; i < size; ++i)
    {    
        threads.emplace_back(std::thread([this](){
           while(true)// 获取任务
           {
            std::function<void()> task;
            {//临界区开始
                std::unique_lock<std::mutex> lock(tasks_mtx);
                cv.wait(lock, [this](){
                    return stop || !tasks.empty();
                });
                if(stop && !tasks.empty()) return;
                task = std::move(tasks.front());
                tasks.pop();
            }//临界区结束
            task();
           }
        }));
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(tasks_mtx);
        stop = true;  // 设置停止标志
    }
    cv.notify_all();        // 欺骗线程启动，开stop，全部杀死
    for(auto& thread : threads)
    {
        thread.join();
    }
}
