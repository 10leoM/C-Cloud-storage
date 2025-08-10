#include "HttpServer.h"
#include "base/Logging.h"

using namespace mymuduo;
using namespace mymuduo::net;

namespace mymuduo {
namespace net {
namespace detail {

bool defaultHttpCallback(const TcpConnectionPtr&, const HttpRequest&, HttpResponse* resp) {
    resp->setStatusCode(HttpResponse::k404NotFound);
    resp->setStatusMessage("Not Found");
    resp->setCloseConnection(true);
    return true;
}

} // namespace detail
} // namespace net
} // namespace mymuduo

HttpServer::HttpServer(EventLoop* loop,
                     const InetAddress& listenAddr,
                     const std::string& name)
    : server_(loop, listenAddr, name),
      httpCallback_(detail::defaultHttpCallback)
{
    server_.setConnectionCallback(
        std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
        std::bind(&HttpServer::onMessage, this, std::placeholders::_1,
                 std::placeholders::_2, std::placeholders::_3));
}

void HttpServer::onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        auto context = std::make_shared<HttpContext>();
        conn->setContext(context);
    }
}

void HttpServer::onMessage(const TcpConnectionPtr& conn,
                         Buffer* buf,
                         Timestamp receiveTime) {
    auto context = std::static_pointer_cast<HttpContext>(conn->getContext());

    if(!context){
        LOG_INFO << "context is null";
        return;
    }

    HttpContext::ParseResult result = context->parseRequest(buf, receiveTime);
    LOG_INFO << "result = " << result;
    if (result == HttpContext::kError) {  // 解析出错
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        conn->shutdown();
        return;
    }

    if (result == HttpContext::kHeadersComplete) {  // 头部解析完成
        // 如果是大文件上传，在这里就可以开始处理了
        if (context->expectBody()) {
            HttpRequest& req = context->request();  // 改为非const引用
            // 检查是否是文件上传请求
            if (req.path() == "/upload" && req.method() == HttpRequest::kPost) {
                // 检查body数据大小
                size_t bufSize = req.body().size();  //这个才是对的
                // LOG_INFO << "bufSize = " << bufSize;
                if (bufSize >= 1024 * 1024) {  // 如果数据超过1MB
                    // LOG_INFO << "Buffer size exceeds 1MB, processing chunk";
                    HttpResponse response(false);  // 不关闭连接
                    bool syncProcessed = httpCallback_(conn, req, &response);
                    if (!syncProcessed) {
                        // 异步处理，不重置 context
                        LOG_INFO << "Async upload chunk processing";
                        return;
                    } else {
                        LOG_INFO << "Sync upload chunk processed";
                    }
                }
            }
        }
    } else if (result == HttpContext::kGotRequest) {  // 整个请求解析完成
        bool syncProcessed = onRequest(conn, context->request());
        if (syncProcessed) {
            LOG_INFO << "context->reset()";
            context->reset();
        }
    } else {
        LOG_INFO << "need more data";
    }
    LOG_DEBUG << "onMessage end";
}

bool HttpServer::onRequest(const TcpConnectionPtr& conn, HttpRequest& req) {
    // LOG_DEBUG << "onRequest start";
    const std::string& connection = req.getHeader("Connection");
    bool close = connection == "close" ||
        (req.getVersion() == HttpRequest::kHttp10 && connection != "Keep-Alive");
    HttpResponse response(close);

    // 调用用户的回调函数处理请求
    bool syncProcessed = httpCallback_(conn, req, &response);

    // 如果是同步处理完成，或者不是异步响应，直接发送响应
    if (syncProcessed) {
        Buffer buf;
        response.appendToBuffer(&buf);
        conn->send(&buf);
        if (response.closeConnection()) {
            conn->shutdown();
        }
        LOG_INFO << "Sync request completed";
    } else {
        LOG_INFO << "Async request, waiting for response";
    }
    
    return syncProcessed;
} 