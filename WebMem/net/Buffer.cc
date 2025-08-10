#include "Buffer.h"
#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

using namespace mymuduo;
using namespace mymuduo::net;

const char Buffer::kCRLF[] = "\r\n";

/// @brief 从文件描述符读取数据
/// @param fd 文件描述符
/// @param savedErrno 保存错误码
/// @return 读取的字节数
ssize_t Buffer::readFd(int fd, int* savedErrno) {
    // 栈上的额外缓冲区
    char extrabuf[65536];
    struct iovec vec[2];
    const size_t writable = writableBytes();

    // 第一块缓冲区是Buffer中的writable空间
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;
    // 第二块缓冲区是栈上的额外缓冲区
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    // 如果Buffer空间足够大，就不使用额外的缓冲区
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if (n < 0) {
        *savedErrno = errno;
    } else if (static_cast<size_t>(n) <= writable) {
        // 数据完全写入Buffer中
        writerIndex_ += n;
    } else {
        // Buffer写满了，额外的数据写入了extrabuf，需要append
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);
    }

    return n;
} 