#ifndef MYMUDUO_BASE_NONCOPYABLE_H
#define MYMUDUO_BASE_NONCOPYABLE_H

namespace mymuduo
{

/// @brief 标记类：表示继承自该类的类是不可拷贝的
/// 通过将拷贝构造函数和赋值运算符声明为private来禁止拷贝
class noncopyable
{
public:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;

protected:
    noncopyable() = default;
    ~noncopyable() = default;
};

}  // namespace mymuduo

#endif  // MYMUDUO_BASE_NONCOPYABLE_H 