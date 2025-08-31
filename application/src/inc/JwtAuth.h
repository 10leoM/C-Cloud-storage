#pragma once

#include <string>
#include <optional>

struct JwtUser
{
    int userId{};
    std::string username;
    std::string jti; // token id
};

// 基于 HS256 的 JWT 封装
class JwtAuth
{
public:
    JwtAuth(std::string secret, int expireSeconds);
    // 生成 JWT 字符串
    std::string issueToken(const JwtUser &u) const;
    // 验签并解析
    std::optional<JwtUser> verifyToken(const std::string &token) const;

private:
    std::string secret_; // JWT 密钥
    int exp_;            // 过期时间（秒）
};
