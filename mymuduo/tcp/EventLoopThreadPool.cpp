#include "EventLoopThreadPool.h"
#include "EventLoop.h"
#include "EventLoopThread.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop *loop):main_reactor_(loop), thread_nums_(0), next_(0) {}

EventLoopThreadPool::~EventLoopThreadPool() {}

void EventLoopThreadPool::SetThreadNums(int thread_nums) 
{
    thread_nums_ = thread_nums;
}

void EventLoopThreadPool::start() 
{
    for (int i = 0; i < thread_nums_; ++i) 
    {
        std::unique_ptr<EventLoopThread> ptr = std::make_unique<EventLoopThread>();
        threads_.emplace_back(std::move(ptr));
        loops_.emplace_back(threads_.back()->StartLoop());
    }
}

EventLoop *EventLoopThreadPool::nextloop() 
{
    EventLoop *ret = main_reactor_;
    if (!loops_.empty()) 
    {
        ret = loops_[next_++];
        if (next_ == static_cast<int>(loops_.size())) 
        {
            next_ = 0;
        }
    }
    return ret;
}