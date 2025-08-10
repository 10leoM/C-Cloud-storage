#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"
#include "EventLoop.h"

#include <stdio.h>
#include <assert.h>

using namespace mymuduo;
using namespace mymuduo::net;

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop,
                                       const string& nameArg)
    : baseLoop_(baseLoop)
    , name_(nameArg)
    , started_(false)
    , numThreads_(0)
    , next_(0) {
    assert(baseLoop != nullptr);
}

EventLoopThreadPool::~EventLoopThreadPool() {
    // 调用stop()确保线程池被正确关闭
    if (started_) {
        stop();
    }
}

void EventLoopThreadPool::start(const ThreadInitCallback& cb) {
    assert(!started_);
    assert(baseLoop_->isInLoopThread());  // 必须在baseLoop线程中启动

    started_ = true;

    // 创建numThreads_个线程
    for (int i = 0; i < numThreads_; ++i) {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
        EventLoopThread* t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        loops_.push_back(t->startLoop());  // 启动EventLoopThread，返回其EventLoop指针
    }

    // 如果numThreads_为0，则在baseLoop_线程中调用cb
    if (numThreads_ == 0 && cb) {
        cb(baseLoop_);
    }
}

void EventLoopThreadPool::stop() {
    assert(started_);
    assert(baseLoop_->isInLoopThread());  // 必须在baseLoop线程中停止

    // 清理所有线程
    for (auto& loop : loops_) {
        // 确保所有pending的回调都被执行
        loop->queueInLoop([loop]() {
            loop->quit();
        });
    }

    // threads_和loops_将在析构时自动清理
    threads_.clear();
    loops_.clear();
    started_ = false;
}

EventLoop* EventLoopThreadPool::getNextLoop() {
    assert(started_);
    EventLoop* loop = baseLoop_;

    // 如果有其他线程，则采用round-robin方式分配
    if (!loops_.empty()) {
        // round-robin
        loop = loops_[next_];
        ++next_;
        if (static_cast<size_t>(next_) >= loops_.size()) {
            next_ = 0;
        }
    }
    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops() {
    assert(started_);
    if (loops_.empty()) {
        return std::vector<EventLoop*>(1, baseLoop_);
    } else {
        return loops_;
    }
} 