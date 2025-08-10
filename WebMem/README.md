# 1 MyMuduo网络库实现Web文件管理
实现muduo网络库，然后基于他做二次开发

# 2 目录结构
```
mymuduo/
├── base/           # 基础设施
├── net/            # 网络核心
│   ├── poller/     # IO复用
│   ├── http/       # HTTP协议实现
├── application/    # 项目代码
``` 

# 3 制作网页图标
convert logo.png  -define icon:auto-resize=256 favicon.ico

# 3 项目编译和运行
## 3.1 导入数据库

```
// 进入项目目录
cd lesson29_file_manager
// 使用用户名 密码导入数据库
mysql -u username -ppassword < file_manager.sql
``` 
比如我username是root，password是123456，即是
```
mysql -u root -p123456 < file_manager.sql

```

数据库的详细设计参考以下内容：

1. 用户表(users)
2. 会话表(sessions)
3. 文件表(files)
4. 文件分享表(file_shares)

以下是完整的SQL语句：

```sql
-- 创建数据库
CREATE DATABASE IF NOT EXISTS file_manager DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

USE file_manager;

-- 创建用户表
CREATE TABLE IF NOT EXISTS users (
    id INT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(50) NOT NULL UNIQUE,
    password VARCHAR(64) NOT NULL,
    email VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 创建会话表
CREATE TABLE IF NOT EXISTS sessions (
    id INT PRIMARY KEY AUTO_INCREMENT,
    session_id VARCHAR(32) NOT NULL UNIQUE,
    user_id INT NOT NULL,
    username VARCHAR(50) NOT NULL,
    expire_time TIMESTAMP NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_session_id (session_id),
    INDEX idx_user_id (user_id),
    INDEX idx_expire_time (expire_time),
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 创建文件表
CREATE TABLE IF NOT EXISTS files (
    id INT PRIMARY KEY AUTO_INCREMENT,
    filename VARCHAR(255) NOT NULL,
    original_filename VARCHAR(255) NOT NULL,
    file_size BIGINT UNSIGNED NOT NULL,
    file_type VARCHAR(50),
    user_id INT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_filename (filename),
    INDEX idx_user_id (user_id),
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 创建文件分享表
CREATE TABLE IF NOT EXISTS file_shares (
    id INT PRIMARY KEY AUTO_INCREMENT,
    file_id INT NOT NULL,
    owner_id INT NOT NULL,
    shared_with_id INT,
    share_type ENUM('private', 'public', 'protected', 'user') NOT NULL,
    share_code VARCHAR(32) NOT NULL,
    expire_time TIMESTAMP NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    extract_code VARCHAR(6),
    INDEX idx_file_id (file_id),
    INDEX idx_owner_id (owner_id),
    INDEX idx_shared_with_id (shared_with_id),
    INDEX idx_share_code (share_code),
    INDEX idx_expire_time (expire_time),
    FOREIGN KEY (file_id) REFERENCES files(id) ON DELETE CASCADE,
    FOREIGN KEY (owner_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (shared_with_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

```

这个SQL脚本包含了以下主要功能：

1. 创建数据库和所有必要的表
2. 设置适当的字符集(utf8mb4)和排序规则(utf8mb4_unicode_ci)
3. 为每个表创建必要的索引以提高查询性能
4. 设置外键约束以确保数据完整性
5. 创建触发器自动清理过期的会话和分享记录

表结构说明：

1. `users` 表：
   - 存储用户基本信息
   - 包含用户名、密码(哈希后)、邮箱等字段
   - 用户名设置为唯一索引

2. `sessions` 表：
   - 存储用户会话信息
   - 包含会话ID、用户ID、过期时间等字段
   - 设置了多个索引以提高查询效率

3. `files` 表：
   - 存储文件信息
   - 包含文件名、原始文件名、文件大小、文件类型等字段
   - 与用户表关联

4. `file_shares` 表：
   - 存储文件分享信息
   - 支持多种分享类型(私有、公开、受保护、指定用户)
   - 包含分享码、提取码、过期时间等字段
   - 与文件和用户表关联

你可以直接执行这个SQL脚本来创建完整的数据库结构。需要注意的是，执行前请确保：

1. MySQL服务已经启动
2. 有足够的权限创建数据库和表
3. 如果数据库已存在，建议先备份数据

如果你需要修改任何表结构或添加新的功能，请告诉我。
       


## 3.2 编译和运行
```
cd lesson29_file_manager
mkdir build
cd build
cmake ..
make
运行：
./bin/http_upload
```