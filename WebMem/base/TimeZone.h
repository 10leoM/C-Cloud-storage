#ifndef MYMUDUO_BASE_TIMEZONE_H
#define MYMUDUO_BASE_TIMEZONE_H

#include "copyable.h"
#include <memory>
#include <time.h>

namespace mymuduo
{

// TimeZone for 1970~2030
class TimeZone : public copyable
{
public:
    TimeZone() = default;  // 无效的时区
    TimeZone(const char* zonefile);  // 从时区文件构造
    TimeZone(int eastOfUtc, const char* tzname);  // 从UTC偏移量构造
    
    // 判断时区是否有效
    bool valid() const { return static_cast<bool>(data_); }

    // 将UTC时间转换为本地时间
    struct tm toLocalTime(time_t secondsSinceEpoch) const;

    // 将本地时间转换为UTC时间
    time_t fromLocalTime(const struct tm&) const;

    // 获取时区名称
    static struct tm utcTime(time_t secondsSinceEpoch);
    static time_t fromUtcTime(const struct tm&);

    struct Data;

private:
    std::shared_ptr<Data> data_;
};

}  // namespace mymuduo

#endif  // MYMUDUO_BASE_TIMEZONE_H 