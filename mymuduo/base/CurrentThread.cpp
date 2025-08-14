// CurrentThread.cpp - 新建文件
#include "CurrentThread.h"

namespace CurrentThread
{
    // 定义线程本地存储变量
    __thread int t_cachedTid = 0;
    __thread char t_formattedTid[32];
    __thread int t_formattedTidLength = 0;

    // 函数定义
    void CacheTid() {
        if (t_cachedTid == 0) {
            t_cachedTid = gettid();
            t_formattedTidLength = snprintf(t_formattedTid, sizeof(t_formattedTid), "%5d ", t_cachedTid);
        }
    }

    pid_t gettid() {
        return static_cast<pid_t>(syscall(SYS_gettid));
    }
}