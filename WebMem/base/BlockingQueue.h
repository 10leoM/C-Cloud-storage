#ifndef MYMUDUO_BASE_BLOCKINGQUEUE_H
#define MYMUDUO_BASE_BLOCKINGQUEUE_H

#include "Condition.h"
#include "Mutex.h"
#include "noncopyable.h"

#include <deque>
#include <assert.h>

namespace mymuduo
{

/**
 * @brief 无界阻塞队列
 * 
 * 特点：
 * 1. 线程安全的队列实现
 * 2. 支持多生产者多消费者
 * 3. 队列为空时，take操作会阻塞
 * 4. 使用互斥锁和条件变量实现同步
 * 5. 无容量限制
 * 
 * @tparam T 队列中元素的类型
 */
template<typename T>
class BlockingQueue : noncopyable
{
public:
    using queue_type = std::deque<T>;

    BlockingQueue()
        : mutex_(),
          notEmpty_(mutex_),
          queue_()
    {
    }

    /**
     * @brief 将元素放入队列
     * @param x 要放入的元素
     * @note 如果有线程在等待take，会唤醒一个
     */
    void put(const T& x)
    {
        MutexLockGuard lock(mutex_);
        queue_.push_back(x);
        notEmpty_.notify(); // 唤醒等待的消费者
    }

    /**
     * @brief 将元素放入队列（移动语义版本）
     * @param x 要放入的元素
     */
    void put(T&& x)
    {
        MutexLockGuard lock(mutex_);
        queue_.push_back(std::move(x));
        notEmpty_.notify();
    }

    /**
     * @brief 从队列中取出一个元素
     * @return 队首元素
     * @note 如果队列为空，会阻塞到有元素可取
     */
    T take()
    {
        MutexLockGuard lock(mutex_);
        // 当队列为空时等待
        while (queue_.empty())
        {
            notEmpty_.wait();
        }
        assert(!queue_.empty());
        T front(std::move(queue_.front()));
        queue_.pop_front();
        return front;
    }

    /**
     * @brief 将队列中所有元素放入指定容器
     * @param queue 目标容器
     * @note 非阻塞操作，如果队列为空，目标容器也为空
     */
    queue_type drain()
    {
        queue_type result;
        {
            MutexLockGuard lock(mutex_);
            result = std::move(queue_);
            assert(queue_.empty());
        }
        return result;
    }

    /**
     * @brief 获取队列中元素个数
     * @return 元素个数
     */
    size_t size() const
    {
        MutexLockGuard lock(mutex_);
        return queue_.size();
    }

private:
    mutable MutexLock mutex_;    // 互斥锁
    Condition notEmpty_;         // 非空条件变量
    queue_type queue_;          // 底层队列容器
};

} // namespace mymuduo

#endif // MYMUDUO_BASE_BLOCKINGQUEUE_H 