#ifndef MYMUDUO_BASE_WEAKCALLBACK_H
#define MYMUDUO_BASE_WEAKCALLBACK_H

#include <functional>
#include <memory>

namespace mymuduo
{

/**
 * @brief 弱回调机制的实现
 * 
 * 用于解决shared_ptr循环引用的问题。通过将对象的weak_ptr和成员函数绑定，
 * 在回调时检查对象是否还存在，避免访问已经销毁的对象。
 * 
 * 使用场景：
 * 1. 对象A持有对象B的shared_ptr
 * 2. 对象B需要回调对象A的成员函数
 * 3. 为避免循环引用，B持有A的weak_ptr
 * 
 * @tparam CLASS 类型
 * @tparam ARGS 参数类型
 */
template<typename CLASS, typename... ARGS>
class WeakCallback
{
public:
    /**
     * @brief 构造函数
     * @param object 对象的weak_ptr
     * @param function 成员函数指针
     */
    WeakCallback(const std::weak_ptr<CLASS>& object,
                const std::function<void(CLASS*, ARGS...)>& function)
        : object_(object),
          function_(function)
    {
    }

    /**
     * @brief 调用操作符重载
     * 
     * 在调用回调函数前，先尝试将weak_ptr提升为shared_ptr，
     * 如果提升成功，说明对象还存在，可以安全调用回调函数；
     * 如果提升失败，说明对象已经被销毁，直接返回。
     * 
     * @param args 参数列表
     */
    void operator()(ARGS&&... args) const
    {
        std::shared_ptr<CLASS> ptr(object_.lock());
        if (ptr)
        {
            function_(ptr.get(), std::forward<ARGS>(args)...);
        }
    }

private:
    std::weak_ptr<CLASS> object_;  // 对象的弱引用
    std::function<void(CLASS*, ARGS...)> function_;  // 回调函数
};

/**
 * @brief 创建弱回调的辅助函数
 * 
 * 使用示例：
 * auto callback = makeWeakCallback(weakPtr, &Class::memberFunction);
 * 
 * @tparam CLASS 类型
 * @tparam ARGS 参数类型
 * @param object 对象的weak_ptr
 * @param function 成员函数指针
 * @return WeakCallback<CLASS, ARGS...> 弱回调对象
 */
template<typename CLASS, typename... ARGS>
WeakCallback<CLASS, ARGS...> makeWeakCallback(const std::weak_ptr<CLASS>& object,
                                            void (CLASS::*function)(ARGS...))
{
    return WeakCallback<CLASS, ARGS...>(object,
                                      std::function<void(CLASS*, ARGS...)>(function));
}

/**
 * @brief 创建弱回调的辅助函数（const成员函数版本）
 * 
 * @tparam CLASS 类型
 * @tparam ARGS 参数类型
 * @param object 对象的weak_ptr
 * @param function const成员函数指针
 * @return WeakCallback<CLASS, ARGS...> 弱回调对象
 */
template<typename CLASS, typename... ARGS>
WeakCallback<CLASS, ARGS...> makeWeakCallback(const std::weak_ptr<CLASS>& object,
                                            void (CLASS::*function)(ARGS...) const)
{
    return WeakCallback<CLASS, ARGS...>(object,
                                      std::function<void(CLASS*, ARGS...)>(function));
}

} // namespace mymuduo

#endif // MYMUDUO_BASE_WEAKCALLBACK_H 