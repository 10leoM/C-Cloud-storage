#ifndef MYMUDUO_BASE_TYPES_H
#define MYMUDUO_BASE_TYPES_H

#include <stdint.h>
#include <string.h>  // memset
#include <string>

namespace mymuduo
{

using std::string;

inline void memZero(void* p, size_t n)
{
    memset(p, 0, n);
}

// 定义固定长度的整型类型
using int8_t = ::int8_t;
using int16_t = ::int16_t;
using int32_t = ::int32_t;
using int64_t = ::int64_t;

using uint8_t = ::uint8_t;
using uint16_t = ::uint16_t;
using uint32_t = ::uint32_t;
using uint64_t = ::uint64_t;

// 模板元编程：检查类型是否是pod类型
template<typename T>
inline T implicit_cast(typename std::remove_reference<T>::type& x) {
    return x;
}

template<typename T>
inline T implicit_cast(const typename std::remove_reference<T>::type& x) {
    return x;
}

}  // namespace mymuduo

#endif  // MYMUDUO_BASE_TYPES_H 