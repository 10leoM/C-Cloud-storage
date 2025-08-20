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
#include <iostream>
#include <fstream>
#include "StaticFileHandler.h"

bool HttpServer::HttpDefaultCallBack(const std::shared_ptr<Connection> &conn, const HttpRequest &request, HttpResponse *resp)
{
    // 简易静态文件: /static/... 或根路径
    if (request.GetMethod() == HttpMethod::kGet || request.GetMethod() == HttpMethod::kHead) {
        if (StaticFileHandler::Handle("./files", request, resp)) {
            if (request.GetMethod() == HttpMethod::kHead && resp->GetBodyType() == HTML_TYPE) {
                // HEAD 对文件：不发送 body，已在外层处理
                resp->SetBody("");
            }
            return true;
        }
    }
    resp->SetStatusCode(HttpStatusCode::NotFound);
    resp->SetStatusMessage("Not Found");
    resp->SetCloseConnection(true);
    resp->SetBodyType(HTML_TYPE);
    resp->SetBody("404 Not Found\n");
    resp->SetContentLength(13);
    resp->SetContentType("text/plain; charset=utf-8");
    return true;
}

HttpServer::HttpServer(EventLoop *loop, const char *ip, const int port, bool auto_close_conn)
    : loop_(loop), auto_close_conn_(auto_close_conn)
{
    server_ = std::make_unique<Server>(loop_, ip, port);
    server_->setOnConnectionCallback(std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
    server_->setMessageCallback(std::bind(&HttpServer::onMessage, this, std::placeholders::_1));
    SetHttpCallback(std::bind(&HttpServer::HttpDefaultCallBack, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    router_ = std::make_unique<RouteTrie>();
}

HttpServer::~HttpServer() {}

void HttpServer::SetHttpCallback(const HttpResponseCallback &cb) { responseCallback_ = std::move(cb); }

void HttpServer::SetOnConnectionCallback(const std::function<void(const ConnectionPtr &)> &cb) { onConnectionCallback_ = cb; }

void HttpServer::start() { server_->start(); }

void HttpServer::onConnection(const ConnectionPtr &conn)
{
    if (onConnectionCallback_) onConnectionCallback_(conn); // 用户自定义连接回调
    if (auto_close_conn_)
        loop_->RunAfter(AUTOCLOSETIMEOUT,std::bind(&HttpServer::ActiveCloseConn, this, std::weak_ptr<Connection>(conn))); // 使用弱引用避免循环引用
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
            if (context->GetCompleteRequest()) 
            {
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

void HttpServer::onRequest(const ConnectionPtr &conn, HttpRequest &request)
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
    } 
    else if(response.GetBodyType() == HttpBodyType::FILE_TYPE) {
        conn->Send(response.GetBeforeBody());
        if (response.HasRange()) {
            off_t start = static_cast<off_t>(response.GetRangeStart());
            size_t len = static_cast<size_t>(response.GetRangeEnd() - response.GetRangeStart() + 1);
            conn->SendFileRange(response.GetFileFd(), start, len);
        } 
        else {
            conn->SendFile(response.GetFileFd(), response.GetContentLength());
        }
        int ret = close(response.GetFileFd());
        if (ret < 0) LOG_ERROR << "HttpServer::onRequest : close filefd failed, errno: " << errno;
    }
    if (response.IsCloseConnection()) conn->HandleClose();
}

void HttpServer::SendDeferredResponse(const ConnectionPtr &conn)
{
    if (!conn || conn->GetState() != connectionState::Connected) return;
    auto context = conn->getContext<HttpContext>();
    if (!context || !context->HasDeferredResponse()) return;

    HttpResponse *resp = context->GetDeferredResponse();
    if (resp->GetBodyType() == HttpBodyType::HTML_TYPE) {
        conn->Send(resp->GetMessage());
    } 
    else if (resp->GetBodyType() == HttpBodyType::FILE_TYPE) {
        conn->Send(resp->GetBeforeBody());
        if (resp->HasRange()) {
            off_t start = static_cast<off_t>(resp->GetRangeStart());
            size_t len = static_cast<size_t>(resp->GetRangeEnd() - resp->GetRangeStart() + 1);
            conn->SendFileRange(resp->GetFileFd(), start, len);
        } 
        else {
            conn->SendFile(resp->GetFileFd(), resp->GetContentLength());
        }
        int ret = close(resp->GetFileFd());
        if (ret < 0) LOG_ERROR << "HttpServer::SendDeferredResponse : close filefd failed errno=" << errno;
    }
    bool closeConn = resp->IsCloseConnection();
    context->ClearDeferredResponse();
    if (closeConn) conn->HandleClose();
}

void HttpServer::SetThreadNums(int thread_nums) { server_->SetThreadPoolSize(thread_nums); }

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

void HttpServer::AddRoute(const std::string &path, const std::string &method, const std::string &handlerName)
{
    if (!router_) router_ = std::make_unique<RouteTrie>();
    router_->addRoute(path, method, handlerName);
}

void HttpServer::RegisterHandler(const std::string &handlerName, const HttpResponseCallback &cb)
{
    route_handlers_[handlerName] = cb;
}

// bool HttpServer::DispatchByRouter(const ConnectionPtr &conn, HttpRequest &request, HttpResponse *resp)
// {
//     if (!router_) return false;
//     RouteMatch m = router_->findRoute(request.GetUrl(), request.GetMethodString());
//     if (m.handler.empty()) return false;
//     auto it = route_handlers_.find(m.handler);
//     if (it == route_handlers_.end()) return false;
//     if (!m.params.empty()) {
//         HttpRequest reqCopy = request;
//         for (const auto &kv : m.params) reqCopy.SetPathParam(kv.first, kv.second);
//         return it->second(conn, reqCopy, resp);
//     }
//     return it->second(conn, request, resp);
// }