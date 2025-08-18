#pragma once

#include <string>
#include <vector>
#include <optional>
#include <mysql/mysql.h>
#include "Db.h"

// 定义用户仓库类

// 定义用户行结构体
struct UserRow{
    int id;
    std::string username;
    std::string email;
    std::string password;
};

class UserRepository{
public:
    explicit UserRepository(Db& db): db_(db) {}
    
    // 按用户名关键字搜索用户，排除指定用户ID，返回结果限制数量
    std::vector<UserRow> searchByUsernameKeywordExcluding(const std::string& keyword, int excludeUserId, int limit=10){
        std::vector<UserRow> out;
        std::string q = "SELECT id, username, email, password FROM users WHERE username LIKE ''%" + db_.escape(keyword) + 
                        "%' AND id != " + std::to_string(excludeUserId) + " LIMIT " + std::to_string(limit);
        if (MYSQL_RES* res = db_.query(q)) {
            MYSQL_ROW row;
            while(row = mysql_fetch_row(res)) {
                UserRow u;
                u.id = std::stoi(row[0]);
                u.username = row[1] ? row[1] : "";
                u.email = row[2] ? row[2] : "";
                u.password = row[3] ? row[3] : ""; // 假设密码也需要返回
                out.push_back(std::move(u));
            }
            mysql_free_result(res);
        }
        return out;
    }

private:
    Db& db_;   // 数据库连接引用
};
