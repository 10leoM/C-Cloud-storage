#include "inc/JwtAuth.h"
#include <chrono>
#include <jwt-cpp/jwt.h>

using namespace std::chrono;

JwtAuth::JwtAuth(std::string secret, int expireSeconds)
    : secret_(std::move(secret)), exp_(expireSeconds) {}

std::string JwtAuth::issueToken(const JwtUser &u) const
{
    auto now = system_clock::now();
    auto exp = now + seconds(exp_);
    auto token = jwt::create()
                     .set_type("JWT")
                     .set_issued_at(now)
                     .set_expires_at(exp)
                     .set_payload_claim("uid", jwt::claim(std::to_string(u.userId)))
                     .set_payload_claim("name", jwt::claim(u.username))
                     .set_payload_claim("jti", jwt::claim(u.jti))
                     .sign(jwt::algorithm::hs256{secret_});
    return token;
}

std::optional<JwtUser> JwtAuth::verifyToken(const std::string &token) const
{
    try
    {
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::hs256{secret_})
                            .leeway(5);
        verifier.verify(decoded);
        JwtUser u{};
        if (decoded.has_payload_claim("uid"))
            u.userId = std::stoi(decoded.get_payload_claim("uid").as_string());
        if (decoded.has_payload_claim("name"))
            u.username = decoded.get_payload_claim("name").as_string();
        if (decoded.has_payload_claim("jti"))
            u.jti = decoded.get_payload_claim("jti").as_string();
        return u;
    }
    catch (...)
    {
        return std::nullopt;
    }
}
