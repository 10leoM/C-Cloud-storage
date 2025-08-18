#pragma once

#include "Db.h"
#include <string>
#include <optional>

// 文件分享记录
struct ShareRecord
{
    int shareId;
    int fileId;
    int ownerId;
    std::optional<int> sharedWithId;        //
    std::string shareType;                  // public/protected/user
    std::string shareCode;                  // 32 chars
    std::optional<std::string> extractCode; // nullable
    std::string createdAt;
    std::optional<std::string> expireTime; // nullable
    // joined file meta
    std::string serverFilename;
    std::string originalFilename;
    uint64_t fileSize;
    std::string fileType;
    int fileOwnerId;
    std::string ownerUsername;
};

class SharesRepository
{
public:
    SharesRepository(Db &db);
    ~SharesRepository();

    std::optional<ShareRecord> getByCode(const std::string &code)
    {
        std::string q =
            "SELECT fs.id AS share_id, fs.file_id, fs.owner_id, fs.shared_with_id, fs.share_type, fs.share_code, fs.extract_code, DATE_FORMAT(fs.created_at, '%Y-%m-%d %H:%i:%s') AS created_at, "
            "DATE_FORMAT(fs.expire_time, '%Y-%m-%d %H:%i:%s') AS expire_time, f.filename AS server_filename, f.original_filename, f.file_size, f.file_type, f.user_id AS file_owner_id, u.username AS owner_username "
            "FROM file_shares fs JOIN files f ON fs.file_id = f.id JOIN users u ON f.user_id = u.id "
            "WHERE fs.share_code = '" +
            db_.escape(code) + "' AND (fs.expire_time IS NULL OR fs.expire_time > NOW())";
        MYSQL_RES *r = db_.query(q);
        if (!r || mysql_num_rows(r) == 0)
        {
            if (r)
                mysql_free_result(r);
            return std::nullopt;
        }
        MYSQL_ROW row = mysql_fetch_row(r);
        ShareRecord rec;
        rec.shareId = std::stoi(row[0]);
        rec.fileId = std::stoi(row[1]);
        rec.ownerId = std::stoi(row[2]);
        rec.sharedWithId = row[3] ? std::optional<int>(std::stoi(row[3])) : std::nullopt;
        rec.shareType = row[4] ? row[4] : "";
        rec.shareCode = row[5] ? row[5] : "";
        rec.extractCode = row[6] ? std::optional<std::string>(row[6]) : std::nullopt;
        rec.createdAt = row[7] ? row[7] : "";
        rec.expireTime = row[8] ? std::optional<std::string>(row[8]) : std::nullopt;
        rec.serverFilename = row[9] ? row[9] : "";
        rec.originalFilename = row[10] ? row[10] : "";
        rec.fileSize = row[11] ? static_cast<uint64_t>(std::stoull(row[11])) : 0ULL;
        rec.fileType = row[12] ? row[12] : "unknown";
        rec.fileOwnerId = row[13] ? std::stoi(row[13]) : 0;
        rec.ownerUsername = row[14] ? row[14] : "";
        mysql_free_result(r);
        return rec;
    }

    // 判断文件是否属于某用户
    bool isFileOwnedBy(int fileId, int userId)
    {
        std::string q = "SELECT 1 FROM files WHERE id = " + std::to_string(fileId) + " AND user_id = " + std::to_string(userId) + " LIMIT 1";
        MYSQL_RES *r = db_.query(q);
        bool ok = (r && mysql_num_rows(r) > 0);
        if (r)
            mysql_free_result(r);
        return ok;
    }

    // 是否已存在分享给指定用户的记录（share_type='user'）
    bool existsUserShare(int fileId, int sharedWithId)
    {
        std::string q = "SELECT 1 FROM file_shares WHERE file_id = " + std::to_string(fileId) +
                        " AND shared_with_id = " + std::to_string(sharedWithId) + " AND share_type = 'user' LIMIT 1";
        MYSQL_RES *r = db_.query(q);
        bool ok = (r && mysql_num_rows(r) > 0);
        if (r)
            mysql_free_result(r);
        return ok;
    }

    // 设为私有：删除该文件的所有分享
    bool setPrivate(int fileId)
    {
        std::string q = "DELETE FROM file_shares WHERE file_id = " + std::to_string(fileId);
        return db_.exec(q);
    }

    // 创建分享，返回 share_id
    std::optional<int> createShare(int fileId,
                                   int ownerId,
                                   const std::optional<int> &sharedWithId,
                                   const std::string &shareType,
                                   const std::string &shareCode,
                                   const std::optional<std::string> &extractCode,
                                   const std::optional<int> &expireHours)
    {
        std::string expireExpr = "NULL";
        if (expireHours && *expireHours > 0)
        {
            expireExpr = "DATE_ADD(NOW(), INTERVAL " + std::to_string(*expireHours) + " HOUR)";
        }
        std::string sharedWithExpr = sharedWithId ? std::to_string(*sharedWithId) : std::string("NULL");
        std::string extractExpr = extractCode ? ("'" + db_.escape(*extractCode) + "'") : std::string("NULL");
        std::string q =
            "INSERT INTO file_shares (file_id, owner_id, shared_with_id, share_type, share_code, extract_code, expire_time) VALUES (" +
            std::to_string(fileId) + ", " + std::to_string(ownerId) + ", " + sharedWithExpr + ", '" + db_.escape(shareType) + "', '" + db_.escape(shareCode) + "', " + extractExpr + ", " + expireExpr + ")";
        if (!db_.exec(q))
            return std::nullopt;
        return static_cast<int>(db_.insertId());
    }

private:
    Db &db_; // 数据库连接
};