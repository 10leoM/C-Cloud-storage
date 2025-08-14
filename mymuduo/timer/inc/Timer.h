#pragma once

#include "Macro.h"
#include "TimeStamp.h"
#include <functional>

class TimeStamp; // 前向声明
class Timer
{
public:
    DISALLOW_COPY_AND_MOVE(Timer);
    Timer(TimeStamp &timestamp, std::function<void()> const &cb, double interval = 0.0);
    ~Timer();

    void ReStart(TimeStamp now); // 重启定时器
    void run() const;            // 执行定时器回调函数
    TimeStamp GetTime() const;   // 获取定时器触发的时间点
    bool IsRepeat() const;       // 是否重复执行

private:
    TimeStamp time_;                 // 定时器触发的时间点
    std::function<void()> callback_; // 定时器回调函数
    bool repeat_;                    // 是否重复执行
    double interval_;                // 重复执行任务的间隔时间
};