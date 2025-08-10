#ifndef MYMUDUO_BASE_DATE_H
#define MYMUDUO_BASE_DATE_H

#include "Types.h"
#include "copyable.h"

struct tm;

namespace mymuduo
{

/// @brief 日期类，用于处理年月日
class Date : public copyable
{
public:
    struct YearMonthDay
    {
        int year;   // [1900, 2500]
        int month;  // [1, 12]
        int day;    // [1, 31]
    };

    /// @brief 构造一个表示当前日期的Date对象
    Date() : julianDayNumber_(0) {}

    /// @brief 构造一个表示特定日期的Date对象
    /// @param year [1900, 2500]
    /// @param month [1, 12]
    /// @param day [1, 31]
    Date(int year, int month, int day);

    /// @brief 从儒略日构造Date对象
    explicit Date(int julianDayNum) : julianDayNumber_(julianDayNum) {}

    /// @brief 从struct tm构造Date对象
    explicit Date(const struct tm&);

    /// @brief 转换为YearMonthDay结构
    YearMonthDay yearMonthDay() const;

    int year() const { return yearMonthDay().year; }
    int month() const { return yearMonthDay().month; }
    int day() const { return yearMonthDay().day; }

    /// @brief 返回一周中的第几天 [0, 6] => [Sunday, Saturday]
    int weekDay() const;

    /// @brief 获取儒略日数
    int julianDayNumber() const { return julianDayNumber_; }

    /// @brief 转换为字符串表示
    string toString() const;

private:
    int julianDayNumber_;  // 儒略日数，用于内部计算
};

inline bool operator<(Date lhs, Date rhs)
{
    return lhs.julianDayNumber() < rhs.julianDayNumber();
}

inline bool operator==(Date lhs, Date rhs)
{
    return lhs.julianDayNumber() == rhs.julianDayNumber();
}

}  // namespace mymuduo

#endif  // MYMUDUO_BASE_DATE_H 