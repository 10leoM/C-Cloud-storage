#pragma once

// 数据库配置类

#include <string>
#include <mysql/mysql.h>

class DbConfig
{
public:
    DbConfig(const std::string &host, const std::string &user, const std::string &password, const std::string &dbname, unsigned int port);
    ~DbConfig();

    bool connect();                           // 建立连接
    bool exec(const std::string &sql);        // 执行 SQL 语句
    MYSQL_RES *query(const std::string &sql); // 查询并返回结果集
    std::string escape(const std::string &s); // 转义字符串

    unsigned long long insertId() const; // 最近一次插入的自增ID

    bool isConnected() const { return mysql_ != nullptr; } // 是否已连接

private:
    MYSQL *mysql_;
    std::string host_;     // 主机
    std::string user_;     // 用户
    std::string password_; // 密码
    std::string dbname_;   // 数据库名
    unsigned int port_;    // 端口号
};
