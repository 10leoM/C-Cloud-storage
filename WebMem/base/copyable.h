#ifndef MYMUDUO_BASE_COPYABLE_H
#define MYMUDUO_BASE_COPYABLE_H

namespace mymuduo
{

/// @brief 标记类：表示继承自该类的类是可拷贝的
/// 这是一个空基类，不包含任何成员
/// 仅用于在语义上标记某个类是可以拷贝的
class copyable
{
protected:
    copyable() = default;
    ~copyable() = default;
};

}  // namespace mymuduo

#endif  // MYMUDUO_BASE_COPYABLE_H 