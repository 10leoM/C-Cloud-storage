#include "Date.h"
#include <stdio.h>
#include <time.h>

namespace mymuduo
{

namespace detail
{

/// @brief 将日期转换为儒略日数
/// 算法来自：http://www.faqs.org/faqs/calendars/faq/part2/
int getJulianDayNumber(int year, int month, int day)
{
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    return day + (153 * m + 2) / 5 + y * 365 + y / 4 - y / 100 + y / 400 - 32045;
}

/// @brief 将儒略日数转换为日期
struct Date::YearMonthDay getYearMonthDay(int julianDayNumber)
{
    int a = julianDayNumber + 32044;
    int b = (4 * a + 3) / 146097;
    int c = a - ((146097 * b) / 4);
    int d = (4 * c + 3) / 1461;
    int e = c - ((1461 * d) / 4);
    int m = (5 * e + 2) / 153;

    Date::YearMonthDay ymd;
    ymd.day = e - ((153 * m + 2) / 5) + 1;
    ymd.month = m + 3 - 12 * (m / 10);
    ymd.year = 100 * b + d - 4800 + (m / 10);
    return ymd;
}

}  // namespace detail

Date::Date(int year, int month, int day)
    : julianDayNumber_(detail::getJulianDayNumber(year, month, day))
{
}

Date::Date(const struct tm& t)
    : julianDayNumber_(detail::getJulianDayNumber(
        t.tm_year + 1900,
        t.tm_mon + 1,
        t.tm_mday))
{
}

string Date::toString() const
{
    char buf[32];
    YearMonthDay ymd = yearMonthDay();
    snprintf(buf, sizeof buf, "%4d-%02d-%02d", ymd.year, ymd.month, ymd.day);
    return buf;
}

Date::YearMonthDay Date::yearMonthDay() const
{
    return detail::getYearMonthDay(julianDayNumber_);
}

int Date::weekDay() const
{
    // 1970-01-01 是星期四
    return (julianDayNumber_ + 4) % 7;
}

}  // namespace mymuduo 