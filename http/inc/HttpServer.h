#pragma once

#include <functional>
#include <memory>
#include <map>
#include <string>
#include <stdio.h>
#include "Macro.h"
#include "RouterTrie.h"

// 自动关闭的时间，以秒为单位
#define AUTOCLOSETIMEOUT 2


class Server;
class HttpRequest;
class HttpResponse;
class EventLoop;
class Connection;
class RouteTrie;

class HttpServer
{
public:
    typedef std::shared_ptr<Connection> ConnectionPtr;
    // 回调签名: (连接, 请求, 响应*) -> bool; true=同步发送; false=异步稍后调用 SendDeferredResponse
    typedef std::function<bool(const ConnectionPtr &, HttpRequest &, HttpResponse *)> HttpResponseCallback;
    DISALLOW_COPY_AND_MOVE(HttpServer);

    HttpServer(EventLoop *loop, const char *ip, const int port, bool auto_close_conn = true);
    ~HttpServer();

    void SetHttpCallback(const HttpResponseCallback &cb);                     // 设置HTTP响应回调函数 (同步/异步)
    void SetOnConnectionCallback(const std::function<void(const ConnectionPtr &)> &cb); // 设置新连接回调函数
    bool HttpDefaultCallBack(const ConnectionPtr &conn, const HttpRequest &request, HttpResponse *resp); // 默认回调, 返回true表示同步发送

    void start(); // 启动服务器

    void onConnection(const ConnectionPtr &conn);                          // 新连接信息
    void onMessage(const ConnectionPtr &conn);                             // 处理接收到的消息，到onRequest函数处理
    void onRequest(const ConnectionPtr &conn, HttpRequest &request); // 处理HTTP请求，支持同步/异步
    void SendDeferredResponse(const ConnectionPtr &conn);                  // 业务异步完成后触发发送，使用定时器延迟发送
    void SetThreadNums(int thread_nums);

    void ActiveCloseConn(std::weak_ptr<Connection> &conn);                 // 主动关闭连接，不控制conn的生命周期，依然由正常的方式进行释放。

    // 路由注册与处理器绑定
    void AddRoute(const std::string &path, const std::string &method, const std::string &handlerName);
    void RegisterHandler(const std::string &handlerName, const HttpResponseCallback &cb);
    // 在自定义回调中可直接调用，尝试按路由分发；若已处理返回 true，否则返回 false
    bool DispatchByRouter(const ConnectionPtr &conn, const HttpRequest &request, HttpResponse *resp);

private:
    EventLoop *loop_;
    std::unique_ptr<Server> server_;
    HttpResponseCallback responseCallback_;
    std::function<void(const ConnectionPtr &)> onConnectionCallback_;   // 新连接回调
    bool auto_close_conn_; // 是否自动关闭连接
    std::unique_ptr<RouteTrie> router_;                                 // 路由树
    std::map<std::string, HttpResponseCallback> route_handlers_;        // handler 名称 -> 业务回调
};
