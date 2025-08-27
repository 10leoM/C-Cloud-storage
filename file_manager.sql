-- 删除数据库
DROP DATABASE file_manager;

-- 创建数据库
CREATE DATABASE IF NOT EXISTS file_manager DEFAULT CHARACTER
SET
    utf8mb4 COLLATE utf8mb4_unicode_ci;

USE file_manager;

-- 创建用户表
CREATE TABLE
    IF NOT EXISTS users (
        id INT PRIMARY KEY AUTO_INCREMENT, -- 用户ID
        username VARCHAR(50) NOT NULL UNIQUE, -- 用户名
        password VARCHAR(64) NOT NULL, -- 密码
        email VARCHAR(100), -- 邮箱
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, -- 创建时间
        updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, -- 更新时间
        INDEX idx_username (username) -- 用户名索引
    ) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COLLATE = utf8mb4_unicode_ci;

-- 创建会话表
CREATE TABLE
    IF NOT EXISTS sessions (
        id INT PRIMARY KEY AUTO_INCREMENT, -- 会话ID
        session_id VARCHAR(32) NOT NULL UNIQUE, -- 会话ID
        user_id INT NOT NULL, -- 用户ID
        username VARCHAR(50) NOT NULL, -- 用户名
        expire_time TIMESTAMP NOT NULL, -- 过期时间
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, -- 创建时间
        INDEX idx_session_id (session_id), -- 会话ID索引
        INDEX idx_user_id (user_id), -- 用户ID索引
        INDEX idx_expire_time (expire_time), -- 过期时间索引
        FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE
    ) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COLLATE = utf8mb4_unicode_ci;

-- 创建文件表
CREATE TABLE
    IF NOT EXISTS files (
        id INT PRIMARY KEY AUTO_INCREMENT, -- 文件ID
        filename VARCHAR(255) NOT NULL, -- 文件名
        original_filename VARCHAR(255) NOT NULL, -- 原始文件名
        file_size BIGINT UNSIGNED NOT NULL, -- 文件大小
        file_type VARCHAR(50), -- 文件类型
        user_id INT NOT NULL, -- 用户ID
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, -- 创建时间
        updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, -- 更新时间
        INDEX idx_filename (filename), -- 文件名索引
        INDEX idx_user_id (user_id), -- 用户ID索引
        FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE
    ) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COLLATE = utf8mb4_unicode_ci;

-- 创建文件分享表
CREATE TABLE
    IF NOT EXISTS file_shares (
        id INT PRIMARY KEY AUTO_INCREMENT, -- 分享记录ID
        file_id INT NOT NULL, -- 文件ID
        owner_id INT NOT NULL, -- 所有者ID
        shared_with_id INT, -- 共享给的用户ID
        share_type ENUM ('private', 'public', 'protected', 'user') NOT NULL, -- 分享类型
        share_code VARCHAR(32) NOT NULL, -- 分享码
        expire_time TIMESTAMP NULL, -- 过期时间
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, -- 创建时间
        extract_code VARCHAR(6) NULL, -- 提取码
        INDEX idx_file_id (file_id), -- 文件ID索引
        INDEX idx_owner_id (owner_id), -- 所有者ID索引
        INDEX idx_shared_with_id (shared_with_id), -- 共享给的用户ID索引
        INDEX idx_share_code (share_code), -- 分享码索引
        INDEX idx_expire_time (expire_time), -- 过期时间索引
        FOREIGN KEY (file_id) REFERENCES files (id) ON DELETE CASCADE,
        FOREIGN KEY (owner_id) REFERENCES users (id) ON DELETE CASCADE,
        FOREIGN KEY (shared_with_id) REFERENCES users (id) ON DELETE CASCADE
    ) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COLLATE = utf8mb4_unicode_ci;