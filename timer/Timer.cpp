#include "Timer.h"
#include "TimeStamp.h"
#include <functional>

Timer::Timer(TimeStamp &timestamp, std::function<void()> const &cb, double interval)
    : time_(timestamp), callback_(std::move(cb)), interval_(interval), repeat_(interval > 0.0) {}

Timer::~Timer(){}

TimeStamp Timer::GetTime() const { return time_; }

bool Timer::IsRepeat() const { return repeat_; }

void Timer::run() const { callback_(); }

void Timer::ReStart(TimeStamp now)
{
    if(repeat_) {
        time_ = TimeStamp::AddTime(now, interval_);
    } else {
        time_ = now; // 如果不重复，则设置为当前时间
    }
}