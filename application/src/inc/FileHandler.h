#pragma once

#include <string>
#include "Db.h"
#include "AuthHandler.h"
#include "FilenameMap.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "HttpUtil.h"
#include "FileRepository.h"
#include "ShareRepository.h"
#include "Connection.h"

// 文件相关处理：先迁移 list/delete；后续再迁移 upload/download
class FileHandler {
public:
    FileHandler(Db& db, AuthHandler& auth, FilenameMap& fmap, const std::string& uploadDir);

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
};

