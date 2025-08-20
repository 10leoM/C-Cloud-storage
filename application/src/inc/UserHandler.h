#pragma once 

#include "Db.h"
#include "AuthHandler.h"

class UserHandler {
public:
    UserHandler(Db &db, AuthHandler &auth) : db_(db), auth_(auth) {}

private:
    Db &db_;
    AuthHandler &auth_;
};