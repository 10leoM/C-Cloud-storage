#pragma once

#include "Db.h"
#include "AuthHandler.h"
#include "StaticHandler.h"
#include <string>
#include "ShareRepository.h"
#include "Connection.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include <memory>

// 负责分享相关的接口：创建分享、通过分享码访问、分享下载、分享信息
class ShareHandler
{
public:
    ShareHandler(Db &db, AuthHandler &auth, StaticHandler &stat, const std::string &uploadDir)
        : db_(db), auth_(auth), staticHandler_(stat), uploadDir(uploadDir), sharesRepo_(db) {}
    ~ShareHandler() = default;

    bool handleShareFile(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp);     // 处理创建分享请求
    bool handleShareAccess(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp);   // 处理通过分享码访问请求
    bool handleShareDownload(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp); // 处理通过分享码下载请求
    bool handleShareInfo(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp);     // 处理分享信息请求

private:
    Db &db_;
    AuthHandler &auth_;
    StaticHandler &staticHandler_;
    std::string uploadDir;
    SharesRepository sharesRepo_;
};
