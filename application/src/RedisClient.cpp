#include "inc/RedisClient.h"
#include "Logger.h"

#if __has_include(<sw/redis++/redis++.h>)
#include <sw/redis++/redis++.h>
#define HAVE_REDIS_PLUS_PLUS 1
#else
#define HAVE_REDIS_PLUS_PLUS 0
#endif

RedisClient::RedisClient(const std::string &uri)
{
#if HAVE_REDIS_PLUS_PLUS
    try
    {
        redis_ = std::make_shared<sw::redis::Redis>(uri);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Redis 连接失败: " << e.what();
    }
#else
    (void)uri;
    LOG_WARN << "未检测到 redis-plus-plus 头文件，RedisClient 将不可用";
#endif
}

std::shared_ptr<sw::redis::Redis> RedisClient::get()
{
    return redis_;
}
