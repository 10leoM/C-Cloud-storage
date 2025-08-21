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
    resp->SetStatusCode(static_cast<HttpStatusCode>(code));
    resp->SetStatusMessage(message);
    resp->SetContentType("application/json");
    resp->AddHeader("Connection", "close");
    resp->SetBody(out.dump());
    if(conn)
        conn->setWriteCompleteCallback([](const std::shared_ptr<Connection>& c){ c->shutdown(); return true;});
}

inline void sendJson(HttpResponse* resp, json &body, const std::shared_ptr<Connection> &conn, HttpStatusCode code = HttpStatusCode::OK) 
{
    resp->SetStatusCode(code);
    resp->SetStatusMessage(code==HttpStatusCode::OK?"OK":"Internal Server Error");
    resp->SetContentType("application/json");
    resp->AddHeader("Connection", "close");
    resp->SetBody(body.dump());
    if (conn) conn->setWriteCompleteCallback([](const std::shared_ptr<Connection>& c){ c->shutdown(); return true;});
}