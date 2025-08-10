#ifndef MYMUDUO_BASE_TIMESTAMP_H
#define MYMUDUO_BASE_TIMESTAMP_H

#include "copyable.h"
#include "Types.h"

#include <string>

namespace mymuduo
{

/// @brief 时间戳类，精确到微秒
/// 内部使用64位整数表示从Unix纪元开始的微秒数
class Timestamp : public copyable
{
public:
    /// @brief 构造一个表示Unix纪元时间的Timestamp
    Timestamp() : microSecondsSinceEpoch_(0) {}

    /// @brief 构造一个表示特定时间的Timestamp
    /// @param microSecondsSinceEpoch Unix纪元开始的微秒数
    explicit Timestamp(int64_t microSecondsSinceEpoch)
        : microSecondsSinceEpoch_(microSecondsSinceEpoch) {}

    /// @brief 交换两个Timestamp
    void swap(Timestamp& that)
    {
        std::swap(microSecondsSinceEpoch_, that.microSecondsSinceEpoch_);
    }

    /// @brief 转换为字符串
    /// @return 格式化的时间字符串
    std::string toString() const;

    /// @brief 转换为格式化的字符串
    /// @return 格式化的时间字符串，精确到微秒
    std::string toFormattedString(bool showMicroseconds = true) const;

    /// @brief 获取微秒时间戳
    int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }

    /// @brief 获取秒级时间戳
    time_t secondsSinceEpoch() const
    {
        return static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);
    }

    /// @brief 获取当前时间
    static Timestamp now();

    /// @brief 获取一个无效的时间戳
    static Timestamp invalid() { return Timestamp(); }

    /// @brief 从Unix时间戳创建Timestamp
    static Timestamp fromUnixTime(time_t t)
    {
        return fromUnixTime(t, 0);
    }

    /// @brief 从Unix时间戳和微秒创建Timestamp
    static Timestamp fromUnixTime(time_t t, int microseconds)
    {
        return Timestamp(static_cast<int64_t>(t) * kMicroSecondsPerSecond + microseconds);
    }

    static const int kMicroSecondsPerSecond = 1000 * 1000;

private:
    int64_t microSecondsSinceEpoch_;
};

/// @brief 比较两个时间戳
inline bool operator<(Timestamp lhs, Timestamp rhs)
{
    return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
}

/// @brief 比较两个时间戳
inline bool operator==(Timestamp lhs, Timestamp rhs)
{
    return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
}

/// @brief 计算两个时间戳的差值
/// @return 相差的秒数
inline double timeDifference(Timestamp high, Timestamp low)
{
    int64_t diff = high.microSecondsSinceEpoch() - low.microSecondsSinceEpoch();
    return static_cast<double>(diff) / Timestamp::kMicroSecondsPerSecond;
}

/// @brief 给时间戳加上秒数
/// @return 新的时间戳
inline Timestamp addTime(Timestamp timestamp, double seconds)
{
    int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
    return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}

}  // namespace mymuduo

#endif  // MYMUDUO_BASE_TIMESTAMP_H 