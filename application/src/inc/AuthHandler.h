#pragma once

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include "Db.h"
#include "Util.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Connection.h"
#include "JwtAuth.h"

// 前向声明以避免强耦合
namespace sw { namespace redis { class Redis; }}

using json = nlohmann::json;

// 负责用户注册/登录/登出与会话校验
class AuthHandler
{
public:
    explicit AuthHandler(Db &db) : db_(db) {}
    ~AuthHandler() = default;

    // 可选注入 JWT 支持
    void setJwt(JwtAuth* jwt) { jwt_ = jwt; }
    void setRedis(const std::shared_ptr<sw::redis::Redis>& r) { redis_ = r; }

    // 处理 HTTP 路由
    bool handleRegister(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp); // 处理注册请求
    bool handleLogin(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp);    // 处理登录请求
    bool handleLogout(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp);   // 处理登出请求

    // 会话接口（供其它 handler 复用）
    bool validateSession(const std::string &sessionId, int &userId, std::string &username); // 验证会话，获取用户ID和用户名
    void endSession(const std::string &sessionId);                                          // 结束会话

    // 统一鉴权：优先使用 Authorization: Bearer <JWT>；否则回退到 X-Session-ID
    bool ensureAuth(HttpRequest &req, int &userId, std::string &username);

private:
    Db &db_;
    JwtAuth* jwt_ {nullptr};
    std::shared_ptr<sw::redis::Redis> redis_{}; // 可选 Redis：用于会话缓存

    std::string generateSessionId();                                                         // 生成会话 ID
    void saveSession(const std::string &sessionId, int userId, const std::string &username); // 保存会话
};
