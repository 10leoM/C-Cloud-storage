#include "AsyncLogging.h"
#include "LogFile.h"

AsyncLogging::AsyncLogging(const char *filepath) : running_(false), filepath_(filepath), latch_(1) 
{
    current_ = std::make_unique<Buffer>();
    next_ = std::make_unique<Buffer>();
}

AsyncLogging::~AsyncLogging() 
{
    if (running_) {
        Stop();
    }
}

void AsyncLogging::Start() 
{
    running_ = true;
    thread_ = std::thread(std::bind(&AsyncLogging::ThreadFunc, this));
    latch_.wait(); // 等待线程启动完成。
}

void AsyncLogging::Stop() 
{
    running_ = false;
    cv_.notify_one(); // 通知后台线程退出。
    thread_.join();   // 等待后台线程退出。
}

void AsyncLogging::Flush() 
{
    fflush(stdout);
}

void AsyncLogging::Append(const char *data, int len)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if(current_->GetAvail() >= len)
    {
        current_->append(data, len);
    }
    else
    {
        buffers_.push_back(std::move(current_));
        if(next_)
        {
            current_ = std::move(next_);
        }
        else
        {
            current_.reset(new Buffer());
        }
        current_->append(data, len);
    }
    cv_.notify_one(); // 通知后台线程有数据可写。
}

void AsyncLogging::ThreadFunc()
{
    latch_.notify(); // 通知主线程，后台线程已启动。

    std::unique_ptr<Buffer> new_current = std::make_unique<Buffer>();
    std::unique_ptr<Buffer> new_next = std::make_unique<Buffer>();

    std::unique_ptr<LogFile> log_file = std::make_unique<LogFile>(filepath_);

    new_current->bzero();
    new_next->bzero();

    std::vector<std::unique_ptr<Buffer>> actives_buffers;;

    while(running_)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(buffers_.empty() && running_)
        {
            // 如果已满缓冲区还没有，则等待片刻
            cv_.wait_until(lock, std::chrono::system_clock::now() + BufferWriteTimeout * std::chrono::milliseconds(1000),[]{ return false; });
        }

        // 将当前的缓冲区加入到已满缓冲区中
        buffers_.push_back(std::move(current_));
        current_ = std::move(new_current);
        if(!next_)
        {
            next_ = std::move(new_next);
            next_->bzero();
        }
        actives_buffers.swap(buffers_);

        // 写入日志文件
        for(auto &buffer : actives_buffers)
        {
            log_file->Write(buffer->GetData(), buffer->GetLen());
            if(log_file->GetWrittenBytes() >= FileMaximumSize)
            {
                log_file.reset(new LogFile(filepath_)); // 创建新的日志文件
            }
        }
        
        if(actives_buffers.size() > 2)
        {
            actives_buffers.resize(2); // 保留两个缓冲区用于后续使用
        }
        if(!new_current)
        {
            assert(actives_buffers.size() >= 1);
            new_current = std::move(actives_buffers.back());
            actives_buffers.pop_back();
            new_current->bzero();
        }
        if(!new_next)
        {
            assert(actives_buffers.size() >= 1);
            new_next = std::move(actives_buffers.back());
            actives_buffers.pop_back();
            new_next->bzero();
        }
        actives_buffers.clear();
    }
}