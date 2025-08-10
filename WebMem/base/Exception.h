#ifndef MYMUDUO_BASE_EXCEPTION_H
#define MYMUDUO_BASE_EXCEPTION_H

#include <exception>
#include <string>

namespace mymuduo
{

/// @brief 异常基类，提供堆栈跟踪功能
class Exception : public std::exception
{
public:
    /// @brief 构造函数
    /// @param what 异常信息
    explicit Exception(const char* what);
    explicit Exception(const std::string& what);
    
    /// @brief 析构函数
    ~Exception() noexcept override = default;

    /// @brief 获取异常信息
    /// @return 异常信息字符串
    const char* what() const noexcept override
    {
        return message_.c_str();
    }

    /// @brief 获取堆栈跟踪信息
    /// @return 堆栈跟踪字符串
    const char* stackTrace() const noexcept
    {
        return stack_.c_str();
    }

private:
    /// @brief 生成当前堆栈跟踪信息
    void fillStackTrace();

    std::string message_;  // 异常信息
    std::string stack_;    // 堆栈跟踪信息
};

}  // namespace mymuduo

#endif  // MYMUDUO_BASE_EXCEPTION_H 