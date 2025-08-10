#pragma once

#include <functional>
#include <memory>
#include <stdio.h>
#include "Macro.h"

// 自动关闭的时间，以秒为单位
#define AUTOCLOSETIMEOUT 2


class Server;
class HttpRequest;
class HttpResponse;
class EventLoop;
class Connection;

class HttpServer
{
public:
    typedef std::shared_ptr<Connection> ConnectionPtr;
    typedef std::function<void(const HttpRequest &, HttpResponse *)> HttpResponseCallback;
    DISALLOW_COPY_AND_MOVE(HttpServer);

    HttpServer(EventLoop *loop, const char *ip, const int port, bool auto_close_conn = true);
    ~HttpServer();

    void SetHttpCallback(const HttpResponseCallback &cb);                     // 设置HTTP响应回调函数
    void HttpDefaultCallBack(const HttpRequest &request, HttpResponse *resp); // 设置默认的HTTP响应回调函数，不做任何处理

    void start(); // 启动服务器

    void onConnection(const ConnectionPtr &conn);                          // 新连接信息
    void onMessage(const ConnectionPtr &conn);                             // 处理接收到的消息，到onRequest函数处理
    void onRequest(const ConnectionPtr &conn, const HttpRequest &request); // 处理HTTP请求

    void SetThreadNums(int thread_nums);

    // 主动关闭连接，不控制conn的生命周期，依然由正常的方式进行释放。
    void ActiveCloseConn(std::weak_ptr<Connection> &conn);

private:
    EventLoop *loop_;
    std::unique_ptr<Server> server_;
    HttpResponseCallback responseCallback_;
    bool auto_close_conn_; // 是否自动关闭连接
};
