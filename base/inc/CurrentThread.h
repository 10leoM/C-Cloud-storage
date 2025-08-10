// CurrentThread.h - 只保留声明
#pragma once

#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace CurrentThread
{
    extern __thread int t_cachedTid;
    extern __thread char t_formattedTid[32];
    extern __thread int t_formattedTidLength;

    // 只保留声明
    void CacheTid();
    pid_t gettid();
    
    // 保留inline函数
    inline int tid() {
        if (__builtin_expect(t_cachedTid == 0, 0)) {
            CacheTid();
        }
        return t_cachedTid;
    }

    inline const char *tidString() { 
        if (__builtin_expect(t_cachedTid == 0, 0)) {
            CacheTid();
        }
        return t_formattedTid; 
    }
    
    inline int tidStringLength() { 
        if (__builtin_expect(t_cachedTid == 0, 0)) {
            CacheTid();
        }
        return t_formattedTidLength; 
    }
}