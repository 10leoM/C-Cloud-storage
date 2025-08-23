#pragma once

#include <string>
#include <memory>
#include "HttpResponse.h"
#include "Connection.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// 工具函数：发送错误响应
// 这里假设 Connection 类有 Send 方法用于发送数据
// 以及 HandleClose 方法用于关闭连接

#pragma once

#include <string>
#include <memory>
#include "HttpResponse.h"
#include "Connection.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// 工具函数：发送错误响应
// 这里假设 Connection 类有 Send 方法用于发送数据
// 以及 HandleClose 方法用于关闭连接

inline void sendError(HttpResponse* resp, const std::string &message, int code, const std::shared_ptr<Connection> &conn) 
{
    json out = {{"code", code}, {"message", message}};
    const std::string body = out.dump();
    resp->SetStatusCode(static_cast<HttpStatusCode>(code));
    if (message.empty()) {
        // 简单映射常见状态
        switch (code) {
            case 400: resp->SetStatusMessage("Bad Request"); break;
            case 401: resp->SetStatusMessage("Unauthorized"); break;
            case 403: resp->SetStatusMessage("Forbidden"); break;
            case 404: resp->SetStatusMessage("Not Found"); break;
            case 416: resp->SetStatusMessage("Range Not Satisfiable"); break;
            case 500: resp->SetStatusMessage("Internal Server Error"); break;
            default: resp->SetStatusMessage("Error"); break;
        }
    } else {
        resp->SetStatusMessage(message);
    }
    resp->SetContentType("application/json");
    resp->SetBody(body);
    resp->SetBodyType(HTML_TYPE);
    resp->SetContentLength(static_cast<int>(body.size()));
    resp->SetCloseConnection(true); // 保守：错误后关闭
    if (conn) conn->setWriteCompleteCallback([](const std::shared_ptr<Connection>& c){ c->shutdown(); return true;});
}

inline void sendJson(HttpResponse* resp, json &body, const std::shared_ptr<Connection> &conn, HttpStatusCode code = HttpStatusCode::OK) 
{
    resp->SetStatusCode(code);
    resp->SetStatusMessage(code==HttpStatusCode::OK?"OK":"Internal Server Error");
    resp->SetContentType("application/json");
    const std::string s = body.dump();
    resp->SetBody(s);
    resp->SetBodyType(HTML_TYPE);
    resp->SetContentLength(static_cast<int>(s.size()));
    // 是否关闭连接由上层根据请求头决定；为兼容前端 fetch，这里也可选择保持连接
    // 不强制添加 Connection: close 头，交由 HttpServer 根据 close_flag 决定
    if (conn) conn->setWriteCompleteCallback([](const std::shared_ptr<Connection>& c){ c->shutdown(); return true;});
}



// inline void sendError(HttpResponse* resp, const std::string &message, int code, const std::shared_ptr<Connection> &conn) 
// {
//     json out = {{"code", code}, {"message", message}};
//     resp->SetStatusCode(static_cast<HttpStatusCode>(code));
//     resp->SetStatusMessage(message);
//     resp->SetContentType("application/json");
//     resp->AddHeader("Connection", "close");
//     resp->SetBody(out.dump());
//     if(conn)
//         conn->setWriteCompleteCallback([](const std::shared_ptr<Connection>& c){ c->shutdown(); return true;});
// }

// inline void sendJson(HttpResponse* resp, json &body, const std::shared_ptr<Connection> &conn, HttpStatusCode code = HttpStatusCode::OK) 
// {
//     resp->SetStatusCode(code);
//     resp->SetStatusMessage(code==HttpStatusCode::OK?"OK":"Internal Server Error");
//     resp->SetContentType("application/json");
//     resp->AddHeader("Connection", "close");
//     resp->SetBody(body.dump());
//     if (conn) conn->setWriteCompleteCallback([](const std::shared_ptr<Connection>& c){ c->shutdown(); return true;});
// }