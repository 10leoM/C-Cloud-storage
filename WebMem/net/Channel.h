#ifndef MYMUDUO_NET_CHANNEL_H
#define MYMUDUO_NET_CHANNEL_H

#include <functional>
#include <memory>
#include "base/Timestamp.h"

namespace mymuduo {
namespace net {

class EventLoop;

/// @brief Channel 是 selectable IO channel 的抽象
/// 每个Channel对象自始至终只属于一个EventLoop
/// 每个Channel对象自始至终只负责一个文件描述符的IO事件分发
/// Channel 的生命期由其owner class负责管理（TcpConnection等）
class Channel {
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    // 防止拷贝
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;

    /// @brief 将Channel和它所属的对象绑定
    void tie(const std::shared_ptr<void>&);

    /// @brief 处理事件
    void handleEvent(Timestamp receiveTime);

    /// @brief 设置读事件回调
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    /// @brief 设置写事件回调
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    /// @brief 设置关闭事件回调
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    /// @brief 设置错误事件回调
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    /// @brief 获取文件描述符
    int fd() const { return fd_; }
    /// @brief 获取注册的事件
    int events() const { return events_; }
    /// @brief 设置实际发生的事件
    void set_revents(int revt) { revents_ = revt; }
    /// @brief 是否没有注册任何事件
    bool isNoneEvent() const { return events_ == kNoneEvent; }

    /// @brief 获取index
    int index() const { return index_; }
    /// @brief 设置index
    void set_index(int idx) { index_ = idx; }

    /// @brief 使能读事件
    void enableReading() { events_ |= kReadEvent; update(); }
    /// @brief 禁用读事件
    void disableReading() { events_ &= ~kReadEvent; update(); }
    /// @brief 使能写事件
    void enableWriting() { events_ |= kWriteEvent; update(); }
    /// @brief 禁用写事件
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    /// @brief 禁用所有事件
    void disableAll() { events_ = kNoneEvent; update(); }
    /// @brief 是否注册了写事件
    bool isWriting() const { return events_ & kWriteEvent; }
    /// @brief 是否注册了读事件
    bool isReading() const { return events_ & kReadEvent; }

    /// @brief 获取所属的EventLoop
    EventLoop* ownerLoop() { return loop_; }
    /// @brief 从EventLoop中移除当前Channel
    void remove();

    // 用于调试
    string reventsToString() const;
    string eventsToString() const;

private:
    /// @brief 将Channel的更改注册到EventLoop
    void update();
    /// @brief 将事件转换为字符串
    static string eventsToString(int fd, int ev);
    /// @brief 实际的事件处理函数
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;   // 无事件
    static const int kReadEvent;   // 读事件
    static const int kWriteEvent;  // 写事件

    EventLoop* loop_;              // 所属的EventLoop
    const int fd_;                 // 文件描述符
    int events_;                   // 注册的事件
    int revents_;                  // 实际发生的事件
    int index_;                    // 用于Poller

    bool eventHandling_;           // 是否正在处理事件
    bool addedToLoop_;            // 是否已添加到EventLoop
    bool logHup_;                 // 是否记录POLLHUP事件

    ReadEventCallback readCallback_;    // 读事件回调
    EventCallback writeCallback_;       // 写事件回调
    EventCallback closeCallback_;       // 关闭事件回调
    EventCallback errorCallback_;       // 错误事件回调

    std::weak_ptr<void> tie_;     // 用于延长所属对象的生命期
    bool tied_;                   // 是否已经绑定
};

}  // namespace net
}  // namespace mymuduo

#endif  // MYMUDUO_NET_CHANNEL_H 