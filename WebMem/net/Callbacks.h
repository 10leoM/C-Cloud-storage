#ifndef MYMUDUO_NET_CALLBACKS_H
#define MYMUDUO_NET_CALLBACKS_H

#include <functional>
#include <memory>
#include "base/Timestamp.h"
#include "net/Buffer.h"

namespace mymuduo {

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

template<typename T>
inline T* get_pointer(const std::shared_ptr<T>& ptr) {
    return ptr.get();
}

template<typename T>
inline T* get_pointer(const std::unique_ptr<T>& ptr) {
    return ptr.get();
}

namespace net {
class TcpConnection;
typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;
// All client visible callbacks go here.
using TimerCallback = std::function<void()>;
using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
using CloseCallback = std::function<void(const TcpConnectionPtr&)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr&, size_t)>;
typedef std::function<void (const TcpConnectionPtr&,
                            Buffer*,
                            Timestamp)> MessageCallback;
}  // namespace net
}  // namespace mymuduo

#endif  // MYMUDUO_NET_CALLBACKS_H 