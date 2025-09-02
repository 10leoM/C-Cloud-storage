#pragma once

#include <memory>
#include <string>

// 轻量 Redis 客户端封装：通过 URI 初始化，提供共享 Redis 连接实例
// 示例 URI: tcp://127.0.0.1:6379?db=0

namespace sw{namespace redis{
    class Redis;
}}

class RedisClient
{
public:
    explicit RedisClient(const std::string &uri);   // uri实例：tcp://127.0.0.1:6379?db=0
    std::shared_ptr<sw::redis::Redis> get();

    sw::redis::Redis* operator->();

private:
    std::shared_ptr<sw::redis::Redis> redis_;   // Redis 连接实例
};
