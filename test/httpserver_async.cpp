#include "HttpServer.h"
#include "EventLoop.h"
#include "Logger.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "HttpContext.h"
#include <memory>
#include <atomic>

// 演示: /async 路径异步处理，先立即返回 false，100ms 后填充响应并发送

bool DemoCallback(const std::shared_ptr<Connection>& conn, const HttpRequest &req, HttpResponse *resp)
{
    if (req.GetUrl() == "/async") {
        // 先占位：告诉客户端将保持连接（这里用 keep-alive 示例）
        resp->SetStatusCode(HttpStatusCode::OK);
        resp->SetStatusMessage("OK");
        resp->SetContentType("text/plain");
        resp->SetContentLength(0); // 暂不发送 body
        // 保存 deferred response
        auto ctx = conn->getContext<HttpContext>();
        if (!ctx) {
            auto newCtx = std::make_shared<HttpContext>();
            conn->setContext(newCtx);
            ctx = newCtx;
        }
        ctx->StoreDeferredResponse(*resp);
        // 捕获连接弱引用, 100ms 后组装真正的 body 并发送
        auto loop = conn->GetLoop();
        std::weak_ptr<Connection> weak = conn;
        loop->RunAfter(0.1, [weak]() {
            if (auto c = weak.lock()) {
                auto ctx = c->getContext<HttpContext>();
                if (ctx && ctx->HasDeferredResponse()) {
                    HttpResponse* r = ctx->GetDeferredResponse();
                    std::string body = "async result\n";
                    r->SetBody(body);
                    r->SetContentLength(body.size());
                    // 使用 HttpServer 的 SendDeferredResponse 需要访问实例，这里简化：直接发送
                    c->Send(r->GetMessage());
                    if (r->IsCloseConnection()) c->HandleClose();
                    ctx->ClearDeferredResponse();
                }
            }
        });
        return false; // 异步
    }
    // 其他路径走同步响应
    std::string body = "sync path: " + req.GetUrl();
    resp->SetStatusCode(HttpStatusCode::OK);
    resp->SetStatusMessage("OK");
    resp->SetBody(body);
    resp->SetContentType("text/plain");
    resp->SetContentLength(body.size());
    return true;
}

int main(int argc, char* argv[]) {
    int port = 8090; // 默认 8090 避免与主 httpserver (8080) 冲突
    if (argc > 1) port = atoi(argv[1]);
    EventLoop loop;
    HttpServer server(&loop, "127.0.0.1", port, false);
    server.SetThreadNums(1);
    server.SetHttpCallback(DemoCallback);

    // 定时将异步响应补齐发送（简单轮询所有连接不方便，这里通过延时任务直接对演示连接发送）。
    // 真实场景可在 DemoCallback 中把连接指针放入某个队列，后台线程完成后再投递 lambda 调用 SendDeferredResponse。

    server.start();
    loop.loop();
    return 0;
}
