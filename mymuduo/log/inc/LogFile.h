#pragma once

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <string>
#include "TimeStamp.h"

static const time_t FlushInterval = 3; // 刷新间隔时间

// 日志文件类
class LogFile
{
public:
    LogFile(const char *filepath = nullptr);
    ~LogFile();

    void Write(const char *data, int len); // 写入日志数据

    void Flush(); // 刷新日志文件

    int64_t GetWrittenBytes() const; // 获取已写入的字节数

private:
    FILE *fp_;                  // 文件指针
    int64_t written_bytes_;     // 已写入的字节数
    time_t lastwrite_;          // 上次写入时间
    time_t lastflush_;          // 上次刷新时间
};