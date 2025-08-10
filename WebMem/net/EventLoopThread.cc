#include "EventLoopThread.h"
#include "EventLoop.h"

using namespace mymuduo;
using namespace mymuduo::net;

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,
                               const std::string& name)
    : loop_(nullptr)
    , exiting_(false)
    , thread_(std::bind(&EventLoopThread::threadFunc, this), name)
    , mutex_()
    , cond_()
    , callback_(cb) {
}

EventLoopThread::~EventLoopThread() {
    exiting_ = true;
    if (loop_ != nullptr) {
        // 如果loop_不为空，说明线程还在运行
        loop_->quit();     // 退出事件循环
        thread_.join();    // 等待线程结束
    }
}

EventLoop* EventLoopThread::startLoop() {
    // 启动新线程
    thread_.start();

    EventLoop* loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // 等待threadFunc创建EventLoop对象
        cond_.wait(lock, [this]() { return loop_ != nullptr; });
        loop = loop_;
    }
    return loop;
}

void EventLoopThread::threadFunc() {
    // 创建一个新的EventLoop对象
    // one loop per thread
    EventLoop loop;

    // 如果有初始化回调，执行回调
    if (callback_) {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;     // loop_指向一个栈上的对象，threadFunc函数退出之后，这个指针就失效了
        cond_.notify_one();
    }

    // 执行事件循环
    loop.loop();  // EventLoop loop  => Poller.poll

    // 事件循环结束，清理
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
} 