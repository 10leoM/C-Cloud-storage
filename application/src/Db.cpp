#include "./inc/Db.h"
#include "Logger.h"
#include <mysql/mysql.h>

Db::Db(const std::string& host,
       const std::string& user,
       const std::string& password,
       const std::string& dbname,
       unsigned int port)
    : mysql_(nullptr), host_(host), user_(user), password_(password), dbname_(dbname), port_(port) {}

Db::~Db() {
    if (mysql_) {
        mysql_close(mysql_);
        mysql_ = nullptr;
    }
}

bool Db::connect() {
    if(mysql_) return true;
    mysql_ = mysql_init(nullptr);
    if (!mysql_) {
        LOG_ERROR << "MySQL init failed";
        return false;
    }
    if(!mysql_real_connect(mysql_, host_.c_str(), user_.c_str(), password_.c_str(), dbname_.c_str(), port_, nullptr, 0)) {
        LOG_ERROR << "MySQL connect failed: " << mysql_error(mysql_);
        mysql_close(mysql_);
        mysql_ = nullptr;
        return false;
    }
    if (mysql_set_character_set(mysql_, "utf8") != 0) {
        LOG_ERROR << "Set charset failed: " << mysql_error(mysql_);
        return false;
    }
    return true;
}

bool Db::exec(const std::string& sql) {
    if (!mysql_) return false;
    if (mysql_query(mysql_, sql.c_str()) != 0) {
        LOG_ERROR << "MySQL exec failed: " << mysql_error(mysql_);
        return false;
    }
    return true;
}

MYSQL_RES* Db::query(const std::string& sql) {
    if (!exec(sql)) return nullptr;
    return mysql_store_result(mysql_);
}

std::string Db::escape(const std::string& s) {
    if (!mysql_) return s;
    std::string out;
    out.resize(s.size() * 2 + 1);
    unsigned long len = mysql_real_escape_string(mysql_, &out[0], s.c_str(), s.size());
    out.resize(len);
    return out;
}

unsigned long long Db::insertId() const {
    if (!mysql_) return 0;
    return mysql_insert_id(mysql_);
}

bool Db::isConnected() const {
    return mysql_ != nullptr;
}