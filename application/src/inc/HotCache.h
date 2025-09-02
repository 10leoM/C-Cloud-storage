#pragma once

#include <sw/redis++/redis++.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include <random>
#include <functional>
#include <optional>
#include <string>
#include "RedisClient.h"

using json = nlohmann::json;

/**
 * Redis 热点数据缓存封装
 * 实现旁路缓存（cache-aside）模式：
 * - 读取时先查缓存，miss 则查数据库并写入缓存
 * - 更新时先写数据库，成功后删除缓存
 * - 防击穿：互斥锁保护
 * - 防穿透：空值占位缓存
 * - 防雪崩：TTL 抖动
 */
class HotCache {
public:
    explicit HotCache(const std::string& redisUri);
    
    // 从缓存获取或加载数据
    // loader: 返回 std::optional<json> 的函数，用于从数据库加载数据
    template<typename Loader>
    std::optional<json> getOrLoad(const std::string& key, int ttlSec, Loader loader);
    
    // 直接从缓存获取
    std::optional<json> get(const std::string& key);
    
    // 设置缓存（带TTL抖动）
    void set(const std::string& key, const json& value, int ttlSec);
    
    // 删除缓存（更新数据库后调用）
    void invalidate(const std::string& key);
    
    // 批量删除（支持模式匹配）
    void invalidatePattern(const std::string& pattern);
    
    // 计数器操作（如下载量、访问量）
    long long incrCount(const std::string& key, int ttlSec = 3600);
    
    // 排行榜操作（ZSET）
    void addToRank(const std::string& rankKey, const std::string& member, double score);
    std::vector<std::pair<std::string, double>> getTopN(const std::string& rankKey, int n);
    
    // 检查 Redis 连接状态
    bool isConnected();

private:
    RedisClient redis_;

    // 互斥锁操作
    bool tryLock(const std::string& lockKey, int timeoutMs);
    void unlock(const std::string& lockKey);
    
    // TTL 抖动，防止缓存雪崩
    static int addJitter(int baseTtl, double jitterRatio = 0.2);
    
    // 生成随机数
    static int randomInt(int min, int max);
};

// 模板实现
template<typename Loader>
std::optional<json> HotCache::getOrLoad(const std::string& key, int ttlSec, Loader loader) {
    // 1. 先尝试从缓存读取
    auto cached = get(key);
    if (cached) {
        return cached;
    }
    
    // 2. 缓存未命中，使用互斥锁防止击穿
    const std::string lockKey = "lock:" + key;
    if (!tryLock(lockKey, 3000)) { // 3秒超时
        // 未获得锁，稍等后再次尝试读缓存
        std::this_thread::sleep_for(std::chrono::milliseconds(50 + randomInt(0, 50)));
        return get(key);
    }
    
    // 3. 获得锁后，再次检查缓存（双重检查）
    cached = get(key);
    if (cached) {
        unlock(lockKey);
        return cached;
    }
    
    // 4. 真正从数据库加载
    try {
        auto data = loader();
        if (data && !data->is_null()) {
            // 有数据，缓存起来
            set(key, *data, ttlSec);
            unlock(lockKey);
            return data;
        } else {
            // 无数据，缓存空占位符防穿透
            json nullPlaceholder = nullptr;
            set(key + ":null", nullPlaceholder, std::min(ttlSec / 10, 60)); // 短TTL
            unlock(lockKey);
            return std::nullopt;
        }
    } catch (...) {
        unlock(lockKey);
        return std::nullopt;
    }
}
