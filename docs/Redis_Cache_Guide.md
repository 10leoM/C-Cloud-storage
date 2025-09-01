# Redis 热点数据缓存集成指南

## 概述

项目已成功集成 Redis 热点数据缓存，使用旁路缓存（cache-aside）模式：
- **读取**：先查缓存，miss 则查数据库并写入缓存
- **更新**：先写数据库，成功后删除缓存
- **防击穿**：互斥锁保护
- **防穿透**：空值占位缓存
- **防雪崩**：TTL 抖动

## 环境配置

### 启动 Redis
```bash
# 方法1：Docker 启动
docker run -d --name redis -p 6379:6379 redis:alpine

# 方法2：系统安装
sudo apt install redis-server
sudo systemctl start redis
```

### 环境变量
```bash
export REDIS_URL="tcp://127.0.0.1:6379?db=0"
export JWT_SECRET="your-secret-key"
export JWT_EXPIRE="604800"  # 7 天
```

## 缓存功能

### 文件列表缓存
- **缓存键**: `user:{userId}:files:{listType}`
- **TTL**: 5分钟 + 抖动
- **失效时机**: 用户上传/删除文件时

### 下载统计
- **文件下载次数**: `file:{filename}:downloads` (30天)
- **用户下载次数**: `user:{userId}:downloads` (24小时)
- **全局下载统计**: `stats:total_downloads` (24小时)

### 热门排行榜
- **热门文件**: `hot:files` (ZSET)
- **活跃用户**: `hot:users` (ZSET)

### 访问计数
- **用户访问次数**: `user:{userId}:access_count` (24小时)
- **删除统计**: `stats:files_deleted` (24小时)

## API 响应变化

### 文件列表 (/files)
现在会包含缓存状态标识：
```json
{
  "code": 0,
  "message": "Success",
  "files": [...],
  "cached": true  // 表示数据来自缓存
}
```

## 监控命令

### Redis 状态检查
```bash
redis-cli info stats | grep hits
redis-cli info memory
redis-cli monitor  # 实时查看命令
```

### 查看缓存内容
```bash
# 查看所有用户文件列表缓存
redis-cli keys "user:*:files:*"

# 查看文件下载统计
redis-cli keys "file:*:downloads"

# 查看热门文件排行榜
redis-cli zrevrange hot:files 0 9 withscores
```

## 性能优化建议

### 1. 缓存命中率优化
- 监控缓存命中率，目标 >80%
- 根据业务调整 TTL 时间
- 考虑预热热点数据

### 2. 内存使用优化
```bash
# 设置 Redis 最大内存
redis-cli config set maxmemory 256mb
redis-cli config set maxmemory-policy allkeys-lru
```

### 3. 连接池配置
生产环境建议使用连接池：
```cpp
sw::redis::ConnectionOptions opts;
opts.host = "127.0.0.1";
opts.port = 6379;

sw::redis::ConnectionPoolOptions pool_opts;
pool_opts.size = 10;
auto redis = sw::redis::Redis(opts, pool_opts);
```

## 故障排查

### 常见问题
1. **Redis 连接失败**
   - 检查 Redis 服务状态
   - 验证 REDIS_URL 格式
   - 确认防火墙设置

2. **缓存未生效**
   - 检查环境变量 REDIS_URL
   - 查看应用日志中的缓存相关信息
   - 使用 `redis-cli monitor` 查看命令执行

3. **内存使用过高**
   - 检查 TTL 设置
   - 清理过期键：`redis-cli flushdb`
   - 调整缓存策略

### 调试模式
启用详细日志：
```bash
# 应用启动时查看缓存初始化日志
./http_upload 2>&1 | grep -E "HotCache|Redis"
```

## 进阶功能

### 1. 集群部署
```cpp
// Redis 集群配置示例
std::vector<std::pair<std::string, int>> nodes = {
    {"127.0.0.1", 7000},
    {"127.0.0.1", 7001},
    {"127.0.0.1", 7002}
};
auto redis = sw::redis::RedisCluster(nodes.begin(), nodes.end());
```

### 2. 数据持久化
```bash
# Redis 配置持久化
redis-cli config set save "900 1 300 10 60 10000"
redis-cli bgsave
```

### 3. 监控告警
- 监控缓存命中率
- 监控内存使用率
- 监控连接数
- 设置关键指标告警

## 扩展缓存场景

基于已有的 `HotCache` 类，可以轻松扩展其他缓存场景：

### 用户信息缓存
```cpp
auto userInfo = cache_->getOrLoad("user:info:" + std::to_string(userId), 3600, [&](){
    return loadUserFromDatabase(userId);
});
```

### 配置缓存
```cpp
auto config = cache_->getOrLoad("system:config", 1800, [&](){
    return loadSystemConfig();
});
```

### API 限流
```cpp
long long requests = cache_->incrCount("rate_limit:user:" + std::to_string(userId), 60);
if (requests > 100) {
    // 限流逻辑
}
```
