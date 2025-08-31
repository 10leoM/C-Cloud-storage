#include "inc/RedisClient.h"
#include "Logger.h"
#include <sw/redis++/redis++.h>

RedisClient::RedisClient(const std::string &uri)
{
    try
    {
        redis_ = std::make_shared<sw::redis::Redis>(uri);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Redis 连接失败: " << e.what();
    }
}

std::shared_ptr<sw::redis::Redis> RedisClient::get()
{
    return redis_;
}
