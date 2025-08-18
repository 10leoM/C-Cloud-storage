#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <mysql/mysql.h>
#include <optional>
#include "Db.h"

struct FileRow
{
    int id;                       // 文件ID
    std::string filename;         // 文件名
    std::string originalFilename; // 原始文件名
    uint64_t size;                // 文件大小
    std::string type;             // 文件类型
    std::string createdAt;        // 创建时间
    bool isOwner;                 // 是否是文件所有者
};

struct ShareInfoRow
{
    std::string shareType;                         // 共享类型
    std::optional<int> shareWithId;                // 共享给的用户ID
    std::optional<std::string> sharedWithUsername; // 共享给的用户名
    std::optional<std::string> shareCode;          // 共享码
    std::optional<std::string> extractCode;        // 提取码
    std::optional<std::string> expireTime;         // 过期时间
};

struct FileBasic
{
    int id;                       // 文件ID
    std::string serverFilename;   // 服务器存储的文件名
    std::string originalFilename; // 原始文件名
    int ownerId;                  // 文件所有者ID
};

class FilesRepository
{
public:
    explicit FilesRepository(Db &db) : db_(db) {}

    std::vector<FileRow> listMyFiles(int userId) // 列出用户的文件
    {
        std::vector<FileRow> out;
        std::string q = "SELECT id, filename, original_filename, file_size, file_type, created_at FROM files WHERE user_id = " + std::to_string(userId);
        if (MYSQL_RES *res = db_.query(q))
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)))
            {
                FileRow fr;
                fr.id = std::stoi(row[0]);
                fr.filename = row[1] ? row[1] : "";
                fr.originalFilename = row[2] ? row[2] : "";
                fr.size = row[3] ? static_cast<uint64_t>(std::stoull(row[3])) : 0ULL;
                fr.type = row[4] ? row[4] : "";
                fr.createdAt = row[5] ? row[5] : "";
                fr.isOwner = true; // 这里假设是所有者
                out.push_back(std::move(fr));
            }
            mysql_free_result(res);
        }
        return out;
    }

    std::vector<FileRow> listSharedFiles(int userId) // 列出共享给用户的文件
    {
        std::vector<FileRow> out;
        std::string q = "SELECT f.id, f.filename, f.original_filename, f.file_size, f.file_type, f.created_at "
                        "FROM files f JOIN file_shares fs ON f.id = fs.file_id WHERE (fs.shared_with_id = " +
                        std::to_string(userId) + " OR fs.share_type = 'public') AND f.user_id != " + std::to_string(userId);
        if (MYSQL_RES *res = db_.query(q))
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)))
            {
                FileRow fr;
                fr.id = std::stoi(row[0]);
                fr.filename = row[1] ? row[1] : "";
                fr.originalFilename = row[2] ? row[2] : "";
                fr.size = row[3] ? static_cast<uint64_t>(std::stoull(row[3])) : 0ULL;
                fr.type = row[4] ? row[4] : "";
                fr.createdAt = row[5] ? row[5] : "";
                fr.isOwner = false; // 这里假设不是所有者
                out.push_back(std::move(fr));
            }
            mysql_free_result(res);
        }
        return out;
    }

    std::vector<FileRow> listAllFiles(int userId) // 列出所有文件（包括我的和共享的）
    {
        std::vector<FileRow> out;
        std::string q =
            "SELECT f.id, f.filename, f.original_filename, f.file_size, f.file_type, f.created_at, CASE WHEN f.user_id = " + std::to_string(userId) + " THEN 1 ELSE 0 END as is_owner, u.username "
            "FROM files f LEFT JOIN file_shares fs ON f.id = fs.file_id LEFT JOIN users u ON fs.shared_with_id = u.id "
            "WHERE f.user_id = " + std::to_string(userId) + " OR fs.shared_with_id = " + std::to_string(userId) + " OR fs.share_type = 'public'";
        if (MYSQL_RES *res = db_.query(q))
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)))
            {
                FileRow fr;
                fr.id = std::stoi(row[0]);
                fr.filename = row[1] ? row[1] : "";
                fr.originalFilename = row[2] ? row[2] : "";
                fr.size = row[3] ? static_cast<uint64_t>(std::stoull(row[3])) : 0ULL;
                fr.type = row[4] ? row[4] : "";
                fr.createdAt = row[5] ? row[5] : "";
                fr.isOwner = (row[6] && std::stoi(row[6]) == 1);
                out.push_back(std::move(fr));
            }
            mysql_free_result(res);
        }
        return out;
    }

    std::optional<ShareInfoRow> getShareInfo(int fileId) // 获取文件的分享信息
    {
        std::string q =
            "SELECT fs.share_type, fs.shared_with_id, u.username, fs.share_code, fs.extract_code, fs.expire_time "
            "FROM file_shares fs LEFT JOIN users u ON fs.shared_with_id = u.id WHERE fs.file_id = " +
            std::to_string(fileId) + " LIMIT 1";

        ShareInfoRow info{};
        MYSQL_RES *res = db_.query(q);
        if (!res || mysql_num_rows(res) == 0)
        {
            if (res)
                mysql_free_result(res);
            return std::nullopt;
        }

        MYSQL_ROW row = mysql_fetch_row(res);
        info.shareType = row[0] ? row[0] : "";
        info.shareWithId = row[1] ? std::optional<int>(std::stoi(row[1])) : std::nullopt;
        info.sharedWithUsername = row[2] ? std::optional<std::string>(row[2]) : std::nullopt;
        info.shareCode = row[3] ? std::optional<std::string>(row[3]) : std::nullopt;
        info.extractCode = row[4] ? std::optional<std::string>(row[4]) : std::nullopt;
        info.expireTime = row[5] ? std::optional<std::string>(row[5]) : std::nullopt;
        mysql_free_result(res);
        return info;
    }

    std::optional<int> getOwnedFileIdByServerFilename(const std::string &serverFilename, int ownerId) // 根据服务器文件名和所有者ID获取文件ID
    {
        std::string q = "SELECT id FROM files WHERE filename = '" + db_.escape(serverFilename) + "' AND user_id = " + std::to_string(ownerId) + " LIMIT 1";
        MYSQL_RES *r = db_.query(q);
        if (!r || mysql_num_rows(r) == 0)
        {
            if (r)
                mysql_free_result(r);
            return std::nullopt;
        }
        MYSQL_ROW row = mysql_fetch_row(r);
        int id = row[0] ? std::stoi(row[0]) : 0;
        mysql_free_result(r);
        if (id <= 0)
        {
            return std::nullopt;
        }
        return id;
    }

    std::optional<FileBasic> getByServerFilename(const std::string &serverFilename)     // 根据服务器文件名获取文件基本信息
    {
        std::string q = "SELECT id, filename, original_filename, user_id FROM files WHERE filename='" + db_.escape(serverFilename) + "' LIMIT 1";
        MYSQL_RES *r = db_.query(q);
        if (!r || mysql_num_rows(r) == 0)
        {
            if (r)
                mysql_free_result(r);
            return std::nullopt;
        }
        MYSQL_ROW row = mysql_fetch_row(r);
        FileBasic f;
        f.id = row[0] ? std::stoi(row[0]) : 0;
        f.serverFilename = row[1] ? row[1] : "";
        f.originalFilename = row[2] ? row[2] : "";
        f.ownerId = row[3] ? std::stoi(row[3]) : 0;
        mysql_free_result(r);
        if (f.id <= 0)
        {
            return std::nullopt;
        }
        return f;
    }

    std::optional<int> createFile(const std::string &serverFilename, const std::string &originalFilename, uint64_t fileSize, const std::string& fileType, int userId)
    {
        std::string q = "INSERT INTO files (filename, original_filename, file_size, file_type, user_id) VALUES ('" +
                        db_.escape(serverFilename) + "', '" + db_.escape(originalFilename) + "', " + std::to_string(fileSize) +
                        ", '" + db_.escape(fileType) + "', " + std::to_string(userId) + ")";
        if (!db_.exec(q))
            return std::nullopt;
        return static_cast<int>(db_.insertId());
    }

    bool deleteSharesByFileId(int fileId) // 删除文件的分享信息
    {
        return db_.exec("DELETE FROM file_shares WHERE file_id = " + std::to_string(fileId));
    }

    bool deleteFileById(int fileId) // 根据文件ID删除文件
    {
        return db_.exec("DELETE FROM files WHERE id = " + std::to_string(fileId));
    }

private:
    Db &db_; // 数据库连接
};