#include "inc/HotCache.h"
#include "Logger.h"
#include <algorithm>

HotCache::HotCache(const std::string& redisUri) : redis_(redisUri) {
    try {
        // 测试连接
        redis_->ping();
        LOG_INFO << "HotCache connected to Redis: " << redisUri;
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to connect to Redis: " << e.what();
        throw;
    }
}

std::optional<json> HotCache::get(const std::string& key) {
    try {
        // 检查空占位符
        if (redis_->exists(key + ":null")) {
            return std::nullopt;
        }

        auto value = redis_->get(key);
        if (value) {
            try {
                return json::parse(*value);
            } catch (const std::exception& e) {
                LOG_WARN << "Failed to parse cached JSON for key " << key << ": " << e.what();
                // 删除损坏的缓存
                redis_->del(key);
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "Redis get error for key " << key << ": " << e.what();
    }
    return std::nullopt;
}

void HotCache::set(const std::string& key, const json& value, int ttlSec) {
    try {
        int actualTTL = addJitter(ttlSec);
        redis_->setex(key, actualTTL, value.dump());
        // LOG_INFO << "Cached key: " << key << " TTL: " << actualTTL << "s";
    } catch (const std::exception& e) {
        LOG_ERROR << "Redis set error for key " << key << ": " << e.what();
    }   
}

void HotCache::invalidate(const std::string& key) {
    try {
        redis_->del(key);
        redis_->del(key + ":null"); // 同时删除空占位符
        // LOG_INFO << "Invalidated cache key: " << key;
    } catch (const std::exception& e) {
        LOG_ERROR << "Redis delete error for key " << key << ": " << e.what();
    }
}

void HotCache::invalidatePattern(const std::string& pattern) {
    try {
        std::vector<std::string> keys;
        redis_->keys(pattern, std::back_inserter(keys));    // keys：匹配的键列表
        if (!keys.empty()) {
            redis_->del(keys.begin(), keys.end());
            LOG_INFO << "Invalidated " << keys.size() << " keys matching pattern: " << pattern;
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "Redis pattern delete error for pattern " << pattern << ": " << e.what();
    }
}

long long HotCache::incrCount(const std::string& key, int ttlSec) {
    try {
        auto count = redis_->incr(key);
        if (count == 1) {
            // 首次创建，设置过期时间
            redis_->expire(key, ttlSec);
        }
        return count;
    } catch (const std::exception& e) {
        LOG_ERROR << "Redis incr error for key " << key << ": " << e.what();
        return 0;
    }
}

void HotCache::addToRank(const std::string& rankKey, const std::string& member, double score) {
    try {
        redis_->zincrby(rankKey, score, member);
        // 设置排行榜过期时间（如果是新创建的）
        if (redis_->zcard(rankKey) == 1) {      // zcard：获取当前成员数量
            redis_->expire(rankKey, 24 * 3600); // 24小时
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "Redis zincrby error for key " << rankKey << ": " << e.what();
    }
}

std::vector<std::pair<std::string, double>> HotCache::getTopN(const std::string& rankKey, int n) {
    std::vector<std::pair<std::string, double>> result;
    try {
        redis_->zrevrange(rankKey, 0, n - 1, std::back_inserter(result));
    } catch (const std::exception& e) {
        LOG_ERROR << "Redis zrevrange error for key " << rankKey << ": " << e.what();
    }
    return result;
}

bool HotCache::isConnected() {
    try {
        redis_->ping();
        return true;
    } catch (...) {
        return false;
    }
}

bool HotCache::tryLock(const std::string& lockKey, int timeoutMs) {
    try {
        auto result = redis_->set(lockKey, "1", std::chrono::milliseconds(timeoutMs), sw::redis::UpdateType::NOT_EXIST);
        return result; // redis++ set 返回 bool 表示是否成功设置
    } catch (const std::exception& e) {
        LOG_ERROR << "Redis lock error for key " << lockKey << ": " << e.what();
        return false;
    }
}

void HotCache::unlock(const std::string& lockKey) {
    try {
        redis_->del(lockKey);
    } catch (const std::exception& e) {
        LOG_WARN << "Redis unlock error for key " << lockKey << ": " << e.what();
    }
}

int HotCache::addJitter(int baseTtl, double jitterRatio) {
    int jitter = static_cast<int>(baseTtl * jitterRatio);
    return baseTtl + randomInt(-jitter, jitter);
}

int HotCache::randomInt(int min, int max) {
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(min, max);
    return dist(gen);
}
