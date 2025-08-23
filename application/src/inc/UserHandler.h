#pragma once 

#include "Db.h"
#include "AuthHandler.h"
#include "Connection.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "UserRepository.h"

// 用户处理类，处理与用户相关的HTTP请求，如搜索用户
class UserHandler {
public:
    UserHandler(Db& db, AuthHandler& auth) : db_(db), auth_(auth), usersRepo_(db) {}

    bool handleSearchUsers(const std::shared_ptr<Connection>& conn, HttpRequest& req, HttpResponse* resp);

private:
    Db& db_;
    AuthHandler& auth_;
    UserRepository usersRepo_;
};
