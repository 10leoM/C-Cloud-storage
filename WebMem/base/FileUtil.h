#ifndef MYMUDUO_BASE_FILEUTIL_H
#define MYMUDUO_BASE_FILEUTIL_H

#include "noncopyable.h"
#include "StringPiece.h"
#include "Types.h"
#include <sys/types.h>
#include <string>

namespace mymuduo
{

using std::string;

// 用于文件名参数的类型
typedef string StringArg;

// 用于类型转换的辅助函数
template<typename To, typename From>
inline To implicit_cast(From const& f) {
    return f;
}

namespace FileUtil
{

/**
 * @brief 读取整个文件内容
 * @param filename 文件名
 * @param content 输出参数，存储文件内容
 * @param sizeLimt 读取大小限制，默认为1MB
 * @return 成功返回true，失败返回false
 */
bool readFile(StringArg filename,
             string* content,
             size_t sizeLimt = 1024*1024);

/**
 * @brief 文件读取类
 * RAII方式管理文件描述符
 */
class ReadSmallFile : noncopyable
{
public:
    ReadSmallFile(StringArg filename);
    ~ReadSmallFile();

    /**
     * @brief 读取文件内容到buffer
     * @param content 输出参数，存储文件内容
     * @param maxSize 读取大小限制
     * @return 成功返回0，失败返回errno
     */
    int readToString(size_t maxSize,
                    string* content,
                    int64_t* fileSize = nullptr,
                    int64_t* modifyTime = nullptr,
                    int64_t* createTime = nullptr);

    /**
     * @brief 读取文件内容到buffer
     * @param buf 输出缓冲区
     * @param size 缓冲区大小
     * @return 成功返回0，失败返回errno
     */
    int readToBuffer(char* buf, int size);

private:
    int fd_;        // 文件描述符
    int err_;       // 错误码
    char buf_[64*1024];  // 64KB缓冲区
};

/**
 * @brief 文件追加写入类
 * RAII方式管理文件描述符
 * 支持按大小滚动日志文件
 */
class AppendFile : noncopyable
{
public:
    explicit AppendFile(StringArg filename);
    ~AppendFile();

    /**
     * @brief 追加写入数据
     * @param logline 要写入的数据
     * @param len 数据长度
     */
    void append(const char* logline, size_t len);

    /**
     * @brief 刷新文件缓冲区到磁盘
     */
    void flush();

    /**
     * @brief 获取已写入字节数
     */
    off_t writtenBytes() const { return writtenBytes_; }

private:
    /**
     * @brief 无锁方式写入数据
     */
    size_t write(const char* logline, size_t len);

    FILE* fp_;              // 文件指针
    char buffer_[64*1024];  // 用户态缓冲区，64KB
    off_t writtenBytes_;    // 已写入字节数
};

} // namespace FileUtil

}  // namespace mymuduo

#endif  // MYMUDUO_BASE_FILEUTIL_H 