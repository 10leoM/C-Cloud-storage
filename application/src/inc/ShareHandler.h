#pragma once 

#include "Db.h"
#include "AuthHandler.h"
#include "StaticHandler.h"

class ShareHandler {
public:
    ShareHandler(Db &db, AuthHandler &auth, StaticHandler &staticHandler, const std::string &staticDir) : db_(db), auth_(auth), staticHandler_(staticHandler), staticDir_(staticDir) {}
private:
    Db &db_;
    AuthHandler &auth_;
    StaticHandler &staticHandler_;
    std::string staticDir_;
};
