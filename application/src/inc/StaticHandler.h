#pragma once

#include <string>
#include <memory>
#include "Connection.h"

// 静态资源处理器

class StaticHandler {
public:
    StaticHandler() = default;
    
    // 处理主页请求
    bool handleIndex(const std::shared_ptr<Connection>& conn, HttpRequest& req, HttpResponse* resp);
    // 处理 favicon.ico 请求
    bool handleFavicon(const std::shared_ptr<Connection>& conn, HttpRequest& req, HttpResponse* resp);
    // 处理 /static/* 静态资源
    bool handleStaticAsset(const std::shared_ptr<Connection>& conn, HttpRequest& req, HttpResponse* resp);
};