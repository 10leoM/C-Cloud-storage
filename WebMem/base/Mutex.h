#ifndef MYMUDUO_BASE_MUTEX_H
#define MYMUDUO_BASE_MUTEX_H

#include "noncopyable.h"
#include <assert.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace mymuduo
{

// 获取线程ID
inline pid_t gettid()
{
    return static_cast<pid_t>(::syscall(SYS_gettid));
}

class MutexLock : noncopyable
{
public:
    MutexLock()
        : holder_(0)
    {
        pthread_mutex_init(&mutex_, NULL);
    }

    ~MutexLock()
    {
        assert(holder_ == 0);
        pthread_mutex_destroy(&mutex_);
    }

    bool isLockedByThisThread() const
    {
        return holder_ == gettid();
    }

    void assertLocked() const
    {
        assert(isLockedByThisThread());
    }

    void lock()
    {
        pthread_mutex_lock(&mutex_);
        assignHolder();
    }

    void unlock()
    {
        unassignHolder();
        pthread_mutex_unlock(&mutex_);
    }

    pthread_mutex_t* getPthreadMutex()
    {
        return &mutex_;
    }

private:
    void unassignHolder()
    {
        holder_ = 0;
    }

    void assignHolder()
    {
        holder_ = gettid();
    }

    pthread_mutex_t mutex_;
    pid_t holder_;

    // 友元类声明
    friend class Condition;
};

// RAII方式使用互斥锁
class MutexLockGuard : noncopyable
{
public:
    explicit MutexLockGuard(MutexLock& mutex)
        : mutex_(mutex)
    {
        mutex_.lock();
    }

    ~MutexLockGuard()
    {
        mutex_.unlock();
    }

private:
    MutexLock& mutex_;
};

}  // namespace mymuduo

// 防止误用临时对象
#define MutexLockGuard(x) error "Missing guard object name"

#endif  // MYMUDUO_BASE_MUTEX_H 