#include "CurrentThread.h"

#include <unistd.h>     // syscall()
#include <sys/syscall.h> // SYS_gettid
#include <stdio.h>      // snprintf()
#include <sys/types.h>  // pid_t
#include <sys/time.h>   // struct timeval

namespace mymuduo
{

namespace CurrentThread
{
    // 初始化线程局部存储变量
    __thread int t_cachedTid = 0;
    __thread char t_tidString[32];
    __thread int t_tidStringLength = 6;
    __thread const char* t_threadName = "unknown";

    /**
     * 获取线程的真实ID
     * 在Linux系统中，通过系统调用SYS_gettid获取
     */
    pid_t gettid()
    {
        return static_cast<pid_t>(::syscall(SYS_gettid));
    }

    void cacheTid()
    {
        if (t_cachedTid == 0)
        {
            // 获取真实的线程ID
            t_cachedTid = gettid();
            // 将线程ID转换为字符串形式
            t_tidStringLength = snprintf(t_tidString, sizeof t_tidString, "%5d ", t_cachedTid);
        }
    }

    bool isMainThread()
    {
        // 主线程的线程ID等于进程ID
        return tid() == ::getpid();
    }

    void sleepUsec(int64_t usec)
    {
        struct timeval tv = { 0, 0 };
        tv.tv_sec = static_cast<time_t>(usec / 1000000);
        tv.tv_usec = static_cast<suseconds_t>(usec % 1000000);
        ::select(0, NULL, NULL, NULL, &tv);
    }

} // namespace CurrentThread

} // namespace mymuduo 