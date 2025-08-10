#ifndef MYMUDUO_BASE_CONDITION_H
#define MYMUDUO_BASE_CONDITION_H

#include "Mutex.h"
#include <pthread.h>

namespace mymuduo
{

class Condition : noncopyable
{
public:
    explicit Condition(MutexLock& mutex)
        : mutex_(mutex)
    {
        pthread_cond_init(&pcond_, NULL);
    }

    ~Condition()
    {
        pthread_cond_destroy(&pcond_);
    }

    void wait()
    {
        pthread_cond_wait(&pcond_, mutex_.getPthreadMutex());
    }

    // returns true if time out, false otherwise.
    bool waitForSeconds(double seconds);

    void notify()
    {
        pthread_cond_signal(&pcond_);
    }

    void notifyAll()
    {
        pthread_cond_broadcast(&pcond_);
    }

private:
    MutexLock& mutex_;
    pthread_cond_t pcond_;
};

}  // namespace mymuduo

#endif  // MYMUDUO_BASE_CONDITION_H 