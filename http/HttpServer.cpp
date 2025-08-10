#include "HttpServer.h"
#include "HttpResponse.h"
#include "HttpRequest.h"
#include "EventLoop.h"
#include "Server.h"
#include "Connection.h"
#include "HttpContext.h"
#include "CurrentThread.h"
#include "Logger.h"
#include <arpa/inet.h>
#include <functional>
#include <iostream>
#include <fstream>

void HttpServer::HttpDefaultCallBack(const HttpRequest &request, HttpResponse *resp)
{
    resp->SetStatusCode(HttpStatusCode::NotFound); // 设置状态码为404 Not Found
    resp->SetStatusMessage("Not Found");           // 设置状态消息为Not Found
    resp->SetCloseConnection(true);                // 设置关闭连接为true
}

HttpServer::HttpServer(EventLoop *loop, const char *ip, const int port, bool auto_close_conn)
    : loop_(loop), auto_close_conn_(auto_close_conn)
{
    server_ = std::make_unique<Server>(loop_, ip, port);
    server_->setOnConnectionCallback(std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
    server_->setMessageCallback(std::bind(&HttpServer::onMessage, this, std::placeholders::_1));
    SetHttpCallback(std::bind(&HttpServer::HttpDefaultCallBack, this, std::placeholders::_1, std::placeholders::_2));
}

HttpServer::~HttpServer() {}

void HttpServer::SetHttpCallback(const HttpResponseCallback &cb)
{
    responseCallback_ = std::move(cb);
}

void HttpServer::start()
{
    server_->start(); // 启动服务器
}

void HttpServer::onConnection(const ConnectionPtr &conn)
{
    // int clnt_fd = conn->GetFd();
    // struct sockaddr_in peeraddr;
    // socklen_t peer_addrlength = sizeof(peeraddr);
    // getpeername(clnt_fd, (struct sockaddr *)&peeraddr, &peer_addrlength);

    // std::cout << CurrentThread::tid()
    //           << " HttpServer::onConnection : new connection "
    //           << "[fd#" << clnt_fd << "]"
    //           << " from " << inet_ntoa(peeraddr.sin_addr) << ":" << ntohs(peeraddr.sin_port)
    //           << std::endl;
    if (auto_close_conn_)
    {
        loop_->RunAfter(AUTOCLOSETIMEOUT,
                        std::bind(&HttpServer::ActiveCloseConn, this, std::weak_ptr<Connection>(conn))); // 使用弱引用避免循环引用
    }
}

void HttpServer::onMessage(const ConnectionPtr &conn)
{
    if (conn->GetState() == connectionState::Connected)
    {
        HttpContext *context = conn->GetContext();
        if (!context->ParseRequest(conn->GetReadBuffer()->Peek(), conn->GetReadBuffer()->GetReadablebytes()))
        {
            std::cout<< "ParseRequest failed, message: " << conn->GetReadBuffer()->PeekAllAsString() << std::endl;
            // 解析请求失败，发送400 Bad Request响应
            conn->Send("HTTP/1.1 400 Bad Request\r\n\r\n");
            conn->HandleClose();
        }
        else
        {
            // 解析成功
            if (context->GetCompleteRequest())
            {
                // 获取完整请求并处理
                onRequest(conn, *context->GetRequest());
                context->ResetContextStatus(); // 重置上下文状态
            }
        }
    }
}

void HttpServer::onRequest(const ConnectionPtr &conn, const HttpRequest &request)
{
    std::string connection_state = request.GetHeader("Connection");
    bool Close = (connection_state == "close" || (request.GetVersion() == HttpVersion::kHttp10 && connection_state != "keep-alive")); // 是否关闭连接

    // 处理文件上传的请求
    if (request.GetHeader("Content-Type").find("multipart/form-data") != std::string::npos)
    {
        // 对文件进行处理
        // 先找到文件名，一般第一个filename位置应该就是文件名的所在地。
        // 从content-type中找到边界
        size_t boundary_index = request.GetHeader("Content-Type").find("boundary");
        std::string boundary = request.GetHeader("Content-Type").substr(boundary_index + std::string("boundary=").size());

        std::string filemessage = request.GetBody();
        size_t begin_index = filemessage.find("filename");
        if(begin_index == std::string::npos ){
            LOG_ERROR << "cant find filename";
            return;
        }
        begin_index += std::string("filename=\"").size();
        size_t end_index = filemessage.find("\"\r\n", begin_index); // 能用

        std::string filename = filemessage.substr(begin_index, end_index - begin_index);

        // 对文件信息的处理
        begin_index = filemessage.find("\r\n\r\n") + 4; //遇到空行，说明进入了文件体
        end_index = filemessage.find(std::string("--") + boundary + "--"); // 对文件内容边界的搜寻

        std::string filedata = filemessage.substr(begin_index, end_index - begin_index);
        // 写入文件
        std::ofstream ofs("../../files/" + filename, std::ios::out | std::ios::app | std::ios::binary);
        ofs.write(filedata.data(), filedata.size());
        ofs.close();
    }

    HttpResponse response(Close);
    responseCallback_(request, &response); // 调用用户设置的回调函数处理请求

    if(response.GetBodyType() == HttpBodyType::HTML_TYPE)
        conn->Send(response.GetMessage()); // 发送响应消息
    else if(response.GetBodyType() == HttpBodyType::FILE_TYPE)
    {
        conn->Send(response.GetBeforeBody()); // 先发送响应头
        conn->SendFile(response.GetFileFd(), response.GetContentLength()); // 发送文件内容

        int ret = close(response.GetFileFd()); // 关闭文件描述符
        if (ret < 0)
        {
            LOG_ERROR << "HttpServer::onRequest : close filefd failed, errno: " << errno;
        }
        else
        {
            LOG_INFO << "HttpServer::onRequest : close filefd success, filefd: " << response.GetFileFd();
        }
    }

    if (response.IsCloseConnection())
        conn->HandleClose(); // 如果响应要求关闭连接，则关闭连接
}

void HttpServer::SetThreadNums(int thread_nums)
{
    server_->SetThreadPoolSize(thread_nums); // 设置线程池大小
}

void HttpServer::ActiveCloseConn(std::weak_ptr<Connection> &conn)
{
    // std::cout<< "ActiveCloseConn called." << std::endl;
    ConnectionPtr connection = conn.lock(); // 获取强引用
    if (connection)
    {
        if (connection->GetTimeStamp() < TimeStamp::Now()) // 超时
        {
            connection->HandleClose(); // 关闭连接
            // std::cout << "Connection closed due to timeout." << std::endl;
        }
        else // 未超时，重置时间戳
        {
            loop_->RunAfter(AUTOCLOSETIMEOUT,
                            std::bind(&HttpServer::ActiveCloseConn, this, std::weak_ptr<Connection>(conn))); // 重新设置超时
        }
    }
}