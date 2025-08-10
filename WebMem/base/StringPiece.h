#ifndef MYMUDUO_BASE_STRINGPIECE_H
#define MYMUDUO_BASE_STRINGPIECE_H

#include <string.h>
#include <string>

namespace mymuduo
{

/// @brief 字符串视图类，用于高效传递字符串
/// 这是一个轻量级的字符串引用，不拥有字符串数据
/// C++17后推荐使用std::string_view替代
class StringPiece
{
public:
    StringPiece()
        : ptr_(nullptr), length_(0) {}

    StringPiece(const char* str)
        : ptr_(str), length_(static_cast<int>(strlen(ptr_))) {}

    StringPiece(const unsigned char* str)
        : ptr_(reinterpret_cast<const char*>(str)),
          length_(static_cast<int>(strlen(ptr_))) {}

    StringPiece(const std::string& str)
        : ptr_(str.data()), length_(static_cast<int>(str.size())) {}

    StringPiece(const char* offset, int len)
        : ptr_(offset), length_(len) {}

    // 转换为std::string_view
    operator std::string_view() const
    {
        return std::string_view(ptr_, length_);
    }

    const char* data() const { return ptr_; }
    int size() const { return length_; }
    bool empty() const { return length_ == 0; }

    void clear()
    {
        ptr_ = nullptr;
        length_ = 0;
    }

    void set(const char* buffer, int len)
    {
        ptr_ = buffer;
        length_ = len;
    }

    void set(const char* str)
    {
        ptr_ = str;
        length_ = static_cast<int>(strlen(str));
    }

    void set(const void* buffer, int len)
    {
        ptr_ = reinterpret_cast<const char*>(buffer);
        length_ = len;
    }

    char operator[](int i) const { return ptr_[i]; }

    void remove_prefix(int n)
    {
        ptr_ += n;
        length_ -= n;
    }

    void remove_suffix(int n)
    {
        length_ -= n;
    }

    bool operator==(const StringPiece& x) const
    {
        return ((length_ == x.length_) &&
                (memcmp(ptr_, x.ptr_, length_) == 0));
    }

    bool operator!=(const StringPiece& x) const
    {
        return !(*this == x);
    }

    std::string as_string() const
    {
        return std::string(data(), size());
    }

    void CopyToString(std::string* target) const
    {
        target->assign(ptr_, length_);
    }

    bool starts_with(const StringPiece& x) const
    {
        return ((length_ >= x.length_) &&
                (memcmp(ptr_, x.ptr_, x.length_) == 0));
    }

private:
    const char* ptr_;    // 指向字符串的指针
    int length_;         // 字符串长度
};

}  // namespace mymuduo

#endif  // MYMUDUO_BASE_STRINGPIECE_H 