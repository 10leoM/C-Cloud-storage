#pragma once

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include "Db.h"
#include "Util.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Connection.h"

using json = nlohmann::json;

// 负责用户注册/登录/登出与会话校验
class AuthHandler
{
public:
    explicit AuthHandler(Db &db) : db_(db) {}
    ~AuthHandler() = default;

    // 处理 HTTP 路由
    bool handleRegister(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp); // 处理注册请求
    bool handleLogin(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp);    // 处理登录请求
    bool handleLogout(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp);   // 处理登出请求

    // 会话接口（供其它 handler 复用）
    bool validateSession(const std::string &sessionId, int &userId, std::string &username); // 验证会话，获取用户ID和用户名
    void endSession(const std::string &sessionId);                                          // 结束会话

private:
    Db &db_;

    std::string generateSessionId();                                                         // 生成会话 ID
    void saveSession(const std::string &sessionId, int userId, const std::string &username); // 保存会话
};
