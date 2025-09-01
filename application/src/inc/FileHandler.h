#pragma once

#include <string>
#include <memory>
#include "Db.h"
#include "AuthHandler.h"
#include "FilenameMap.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "HttpUtil.h"
#include "FileRepository.h"
#include "ShareRepository.h"
#include "HotCache.h"
#include "Connection.h"

// 文件相关处理：先迁移 list/delete；后续再迁移 upload/download
class FileHandler {
public:
    FileHandler(Db& db, AuthHandler& auth, FilenameMap& fmap, const std::string& uploadDir);
    
    // 设置缓存实例
    void setCache(std::shared_ptr<HotCache> cache) { cache_ = cache; }

    // 列出用户文件
    bool handleListFiles(const std::shared_ptr<Connection>& conn, HttpRequest& req, HttpResponse* resp);

    // 删除文件
    bool handleDelete(const std::shared_ptr<Connection>& conn, HttpRequest& req, HttpResponse* resp);

    // 下载文件
    bool handleDownload(const std::shared_ptr<Connection>& conn, HttpRequest& req, HttpResponse* resp);

    // 上传文件
    bool handleUpload(const std::shared_ptr<Connection>& conn, HttpRequest& req, HttpResponse* resp);

private:
    Db& db_;
    AuthHandler& auth_;
    FilenameMap& fmap_;
    std::string uploadDir_;
    FilesRepository filesRepo_;
    SharesRepository sharesRepo_; // 复用 Share 的判定逻辑与记录
    std::shared_ptr<HotCache> cache_; // Redis 缓存
    
    // 缓存相关的私有方法
    std::string getUserFileListCacheKey(int userId, const std::string& listType);
    std::string getFileMetaCacheKey(int fileId);
    void invalidateUserCache(int userId);
    
    // 数据库查询方法
    std::optional<nlohmann::json> loadUserFileList(int userId, const std::string& listType);
};

