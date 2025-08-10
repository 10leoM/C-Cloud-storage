#include "Acceptor.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "base/Logging.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "SocketsOps.h"

using namespace mymuduo;
using namespace mymuduo::net;

// 创建一个非阻塞的监听socket
// 设置SO_REUSEADDR和SO_REUSEPORT选项
// 绑定到指定地址
// 设置Channel的可读回调函数
Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
    : loop_(loop),  // 记录所属的事件循环
      acceptSocket_(sockets::createNonblockingOrDie(listenAddr.family())),  // 创建非阻塞socket
      acceptChannel_(loop, acceptSocket_.fd()),  // 创建接受通道
      listening_(false),  // 初始状态为未监听
      idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))  // 打开空闲文件描述符,用于防止文件描述符耗尽
{
    assert(idleFd_ >= 0);
    
    // 设置socket选项
    acceptSocket_.setReuseAddr(true);  // 设置地址重用,防止服务器重启时bind失败
    acceptSocket_.setReusePort(reuseport);  // 设置端口重用,支持多进程监听同一端口
    acceptSocket_.bindAddress(listenAddr);  // 绑定监听地址
    
    // 设置Channel的可读回调函数
    // 当监听socket可读时(有新连接到达),调用handleRead
    acceptChannel_.setReadCallback(
        std::bind(&Acceptor::handleRead, this));
}

// 析构函数
// 关闭所有资源
Acceptor::~Acceptor() {
    acceptChannel_.disableAll();  // 禁用所有事件
    acceptChannel_.remove();  // 从事件循环中移除
    ::close(idleFd_);  // 关闭空闲文件描述符
}

// 开始监听
// 必须在IO线程中调用
void Acceptor::listen() {
    loop_->assertInLoopThread();  // 确保在IO线程中调用
    listening_ = true;  // 设置状态为正在监听
    acceptSocket_.listen();  // 开始监听
    acceptChannel_.enableReading();  // 启用读事件,开始接受连接
}

// 处理新连接到达
// 在IO线程中调用
void Acceptor::handleRead() {
    loop_->assertInLoopThread();  // 确保在IO线程中调用
    InetAddress peerAddr;  // 对端地址
    
    int connfd = acceptSocket_.accept(&peerAddr);  // 接受新连接
    if (connfd >= 0) {  // 接受成功
        if (newConnectionCallback_) {  // 如果设置了回调函数
            newConnectionCallback_(connfd, peerAddr);  // 调用回调函数处理新连接
        } else {
            sockets::close(connfd);  // 如果没有设置回调函数,直接关闭连接
        }
    } else {  // 接受失败
        LOG_SYSERR << "in Acceptor::handleRead";
        // 如果是文件描述符耗尽(errno == EMFILE)
        // 则关闭空闲描述符,接受连接后立即关闭,这样可以优雅地处理文件描述符耗尽的情况
        if (errno == EMFILE) {
            ::close(idleFd_);  // 关闭空闲描述符
            idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);  // 接受连接
            ::close(idleFd_);  // 关闭连接
            idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);  // 重新打开空闲描述符
        }
    }
}

 