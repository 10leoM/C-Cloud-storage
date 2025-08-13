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

bool HttpServer::HttpDefaultCallBack(const std::shared_ptr<Connection> &/*conn*/, const HttpRequest &request, HttpResponse *resp)
{
    resp->SetStatusCode(HttpStatusCode::NotFound); // 设置状态码为404 Not Found
    resp->SetStatusMessage("Not Found");           // 设置状态消息为Not Found
    resp->SetCloseConnection(true);                // 设置关闭连接为true
    return true; // 同步立即返回
}

HttpServer::HttpServer(EventLoop *loop, const char *ip, const int port, bool auto_close_conn)
    : loop_(loop), auto_close_conn_(auto_close_conn)
{
    server_ = std::make_unique<Server>(loop_, ip, port);
    server_->setOnConnectionCallback(std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
    server_->setMessageCallback(std::bind(&HttpServer::onMessage, this, std::placeholders::_1));
    SetHttpCallback(std::bind(&HttpServer::HttpDefaultCallBack, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
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
        auto context = conn->getContext<HttpContext>();
        if (!context)
        {
            context = std::make_shared<HttpContext>();
            conn->setContext(context);
        }
        // 支持 HTTP pipelining: 循环解析缓冲中的多个请求
        while (true) {
            if (!context->HeadersComplete() || !context->BodyComplete()) {
                size_t consumed = 0;
                if (!context->ParseIncremental(conn->GetReadBuffer()->Peek(), conn->GetReadBuffer()->GetReadablebytes(), consumed)) {
                    conn->Send("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n");
                    conn->HandleClose();
                    return;
                }
                if (consumed) conn->GetReadBuffer()->Retrieve(consumed);
            }
            if (context->GetCompleteRequest()) {
                onRequest(conn, *context->GetRequest());
                // 如果连接已被业务标记关闭则不再解析后续
                if (conn->GetState() != connectionState::Connected) return;
                context->ResetContextStatus();
                // 若缓冲区尚有数据则继续下一轮；否则退出
                if (conn->GetReadBuffer()->GetReadablebytes() == 0) break;
                continue; // 尝试解析下一条
            }
            // 未完成且没有更多数据可读，等待下一次 onMessage
            break;
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
    bool done = responseCallback_(conn, request, &response); // true 表示同步返回
    auto context = conn->getContext<HttpContext>();
    if (!done) 
    {
        // 异步: 保存响应对象, 业务稍后填充后调用 SendDeferredResponse
        context->StoreDeferredResponse(response);
        return;
    }

    // 同步回包
    if(response.GetBodyType() == HttpBodyType::HTML_TYPE) {
        conn->Send(response.GetMessage());
    } else if(response.GetBodyType() == HttpBodyType::FILE_TYPE) 
    {
        conn->Send(response.GetBeforeBody());
        conn->SendFile(response.GetFileFd(), response.GetContentLength());
        int ret = close(response.GetFileFd());
        if (ret < 0) 
            LOG_ERROR << "HttpServer::onRequest : close filefd failed, errno: " << errno;
        else 
            LOG_INFO << "HttpServer::onRequest : close filefd success, filefd: " << response.GetFileFd();
    }
    if (response.IsCloseConnection()) conn->HandleClose();
}

void HttpServer::SendDeferredResponse(const ConnectionPtr &conn)
{
    if (!conn || conn->GetState() != connectionState::Connected) return;
    auto context = conn->getContext<HttpContext>();
    if (!context || !context->HasDeferredResponse()) return;
    
    HttpResponse *resp = context->GetDeferredResponse();
    if (resp->GetBodyType() == HttpBodyType::HTML_TYPE) 
    {
        conn->Send(resp->GetMessage());
    } 
    else if (resp->GetBodyType() == HttpBodyType::FILE_TYPE) 
    {
        conn->Send(resp->GetBeforeBody());
        conn->SendFile(resp->GetFileFd(), resp->GetContentLength());
        int ret = close(resp->GetFileFd());
        if (ret < 0) 
            LOG_ERROR << "HttpServer::SendDeferredResponse : close filefd failed errno=" << errno;
    }
    bool closeConn = resp->IsCloseConnection();
    context->ClearDeferredResponse();
    if (closeConn) conn->HandleClose();
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