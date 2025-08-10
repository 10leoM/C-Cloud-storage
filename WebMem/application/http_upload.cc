#include "net/HttpServer.h"
#include "net/HttpRequest.h"
#include "net/HttpResponse.h"
#include "net/EventLoop.h"
#include "net/HttpContext.h"
#include "base/ThreadPool.h"
#include "base/Logging.h"
#include <json/json.h>

#include <iostream>
#include <map>
#include <string>
#include <memory>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <fstream>
#include <experimental/filesystem>
#include <regex>
#include <random>
#include <atomic>
#include <vector>
#include <cstdio>
#include <unistd.h>
#include <mysql/mysql.h>
#include <sstream>

using namespace mymuduo;
using namespace mymuduo::net;

namespace fs = std::experimental::filesystem;

// 文件上传上下文
class FileUploadContext {
public:
    enum class State {
        kExpectHeaders,    // 等待头部
        kExpectContent,    // 等待文件内容
        kExpectBoundary,   // 等待边界
        kComplete         // 上传完成
    };

    FileUploadContext(const std::string& filename, const std::string& originalFilename)
        : filename_(filename)
        , originalFilename_(originalFilename)
        , totalBytes_(0)
        , state_(State::kExpectHeaders)
        , boundary_("")
    {
        // 确保目录存在
        fs::path filePath(filename_);
        fs::path dir = filePath.parent_path();
        if (!dir.empty() && !fs::exists(dir)) {
            fs::create_directories(dir);
        }

        // 打开文件
        file_.open(filename_, std::ios::binary | std::ios::out);
        if (!file_.is_open()) {
            LOG_ERROR << "Failed to open file: " << filename;
            throw std::runtime_error("Failed to open file: " + filename);
        }
        LOG_INFO << "Creating file: " << filename << ", original name: " << originalFilename;
    }

    ~FileUploadContext() {
        if (file_.is_open()) {
            file_.close();
        }
    }

    void writeData(const char* data, size_t len) {
        if (!file_.is_open()) {
            throw std::runtime_error("File is not open: " + filename_);
        }
        
        if (!file_.write(data, len)) {
            throw std::runtime_error("Failed to write to file: " + filename_);
        }
        
        file_.flush();  // 确保数据写入磁盘
        totalBytes_ += len;
        // LOG_INFO << "Wrote " << len << " bytes, total: " << totalBytes_;
    }

    uintmax_t getTotalBytes() const { return totalBytes_; }
    const std::string& getFilename() const { return filename_; }
    const std::string& getOriginalFilename() const { return originalFilename_; }

    void setBoundary(const std::string& boundary) { boundary_ = boundary; }
    State getState() const { return state_; }
    void setState(State state) { state_ = state; }
    const std::string& getBoundary() const { return boundary_; }

private:
    std::string filename_;        // 保存在服务器上的文件名
    std::string originalFilename_; // 原始文件名
    std::ofstream file_;
    uintmax_t totalBytes_;
    State state_;                 // 当前状态
    std::string boundary_;        // multipart边界
};

// 文件下载上下文
class FileDownContext {
public:
    FileDownContext(const std::string& filepath, const std::string& originalFilename)
        : filepath_(filepath)
        , originalFilename_(originalFilename)
        , fileSize_(0)
        , currentPosition_(0)
        , isComplete_(false)
    {
        // 获取文件大小
        fileSize_ = fs::file_size(filepath_);
        
        // 打开文件
        file_.open(filepath_, std::ios::binary | std::ios::in);
        if (!file_.is_open()) {
            LOG_ERROR << "Failed to open file: " << filepath_;
            throw std::runtime_error("Failed to open file: " + filepath_);
        }
        LOG_INFO << "Opening file for download: " << filepath_ << ", size: " << fileSize_;
    }

    ~FileDownContext() {
        if (file_.is_open()) {
            file_.close();
        }
    }

    void seekTo(uintmax_t position) {
        if (!file_.is_open()) {
            throw std::runtime_error("File is not open: " + filepath_);
        }
        file_.seekg(position);
        currentPosition_ = position;
        isComplete_ = false;
    }

    bool readNextChunk(std::string& chunk) {
        if (!file_.is_open() || isComplete_) {
            return false;
        }

        const uintmax_t chunkSize = 1024 * 1024; // 1MB
        uintmax_t remainingBytes = fileSize_ - currentPosition_;
        uintmax_t bytesToRead = std::min(chunkSize, remainingBytes);

        if (bytesToRead == 0) {
            isComplete_ = true;
            return false;
        }

        std::vector<char> buffer(bytesToRead);
        file_.read(buffer.data(), bytesToRead);
        chunk.assign(buffer.data(), bytesToRead);
        currentPosition_ += bytesToRead;

        LOG_INFO << "Read chunk of " << bytesToRead << " bytes, current position: " 
                 << currentPosition_ << "/" << fileSize_;
        return true;
    }

    bool isComplete() const { return isComplete_; }
    uintmax_t getCurrentPosition() const { return currentPosition_; }
    uintmax_t getFileSize() const { return fileSize_; }
    const std::string& getOriginalFilename() const { return originalFilename_; }

private:
    std::string filepath_;        // 文件路径
    std::string originalFilename_; // 原始文件名
    std::ifstream file_;          // 文件流
    uintmax_t fileSize_;          // 文件总大小，使用 uintmax_t 替代 size_t
    uintmax_t currentPosition_;   // 当前读取位置，使用 uintmax_t 替代 size_t
    bool isComplete_;             // 是否完成
};

class HttpUploadHandler {
private:
    ThreadPool threadPool_;              // 线程池
    std::string uploadDir_;             // 上传目录
    std::string mappingFile_;           // 文件名映射文件
    std::atomic<int> activeRequests_;   // 活跃请求计数
    std::mutex mappingMutex_;           // 保护文件名映射的互斥锁
    std::map<std::string, std::string> filenameMapping_;  // 文件名映射 <服务器文件名, 原始文件名>

    // MySQL连接
    MYSQL* mysql;
    std::string dbHost;
    std::string dbUser;
    std::string dbPassword;
    std::string dbName;
    unsigned int dbPort;

    // 定义处理函数类型
    using RequestHandler = bool (HttpUploadHandler::*)(const TcpConnectionPtr&, HttpRequest&, HttpResponse*);
    
    // 路由模式结构
    struct RoutePattern {
        std::regex pattern;              // 正则表达式模式
        std::vector<std::string> params; // 路径参数名列表
        RequestHandler handler;          // 处理函数
        HttpRequest::Method method;      // HTTP方法

        RoutePattern(const std::string& pattern_str, 
                    const std::vector<std::string>& param_names,
                    RequestHandler h,
                    HttpRequest::Method m)
            : pattern(pattern_str)
            , params(param_names)
            , handler(h)
            , method(m)
        {}
    };

    // 路由表
    std::vector<RoutePattern> routes_;

    // 初始化数据库连接
    bool initDatabase() {
        mysql = mysql_init(NULL);
        if (!mysql) {
            LOG_ERROR << "MySQL初始化失败";
            return false;
        }

        if (!mysql_real_connect(mysql, dbHost.c_str(), dbUser.c_str(), 
                              dbPassword.c_str(), dbName.c_str(), dbPort, NULL, 0)) {
            LOG_ERROR << "连接到MySQL服务器失败: " << mysql_error(mysql);
            mysql_close(mysql);
            mysql = NULL;
            return false;
        }

        // 设置字符集为utf8
        if (mysql_set_character_set(mysql, "utf8") != 0) {
            LOG_ERROR << "设置字符集失败: " << mysql_error(mysql);
            return false;
        }

        LOG_INFO << "数据库连接成功";
        return true;
    }

    // 关闭数据库连接
    void closeDatabase() {
        if (mysql) {
            mysql_close(mysql);
            mysql = NULL;
        }
    }

    // 执行SQL查询
    bool executeQuery(const std::string& query) {
        if (!mysql) {
            LOG_ERROR << "数据库未连接";
            return false;
        }

        if (mysql_query(mysql, query.c_str()) != 0) {
            LOG_ERROR << "执行查询失败: " << mysql_error(mysql);
            return false;
        }
        return true;
    }

    // 执行SQL查询并获取结果
    MYSQL_RES* executeQueryWithResult(const std::string& query) {
        if (!executeQuery(query)) {
            return NULL;
        }
        return mysql_store_result(mysql);
    }

public:
    HttpUploadHandler(int numThreads, 
                     const std::string& dbHost = "localhost",
                     const std::string& dbUser = "root",
                     const std::string& dbPassword = "123456",
                     const std::string& dbName = "file_manager",
                     unsigned int dbPort = 3306)
        : threadPool_("UploadHandler")
        , uploadDir_("uploads")
        , mappingFile_("uploads/filename_mapping.json")
        , activeRequests_(0)
        , dbHost(dbHost)
        , dbUser(dbUser)
        , dbPassword(dbPassword)
        , dbName(dbName)
        , dbPort(dbPort)
        , mysql(NULL)
    {
        threadPool_.start(numThreads);
        
        // 创建上传目录
        if (!fs::exists(uploadDir_)) {
            fs::create_directory(uploadDir_);
        }

        // 初始化数据库连接
        if (!initDatabase()) {
            LOG_ERROR << "数据库初始化失败，系统可能无法正常工作";
        }

        // 加载文件名映射
        loadFilenameMapping();
        
        // 初始化路由表
        initRoutes();
    }

    ~HttpUploadHandler() {
        threadPool_.stop();
        // 保存文件名映射
        saveFilenameMapping();
        closeDatabase();
    }

    void onConnection(const TcpConnectionPtr& conn) {
        if (conn->connected()) {
            LOG_INFO << "New connection from " << conn->peerAddress().toIpPort();
            // 为每个新连接创建一个 HttpContext
            conn->setContext(std::make_shared<HttpContext>());
        } else {
            LOG_INFO << "Connection closed from " << conn->peerAddress().toIpPort();
            // 清理上下文
            if (auto context = std::static_pointer_cast<HttpContext>(conn->getContext())) {
                if (auto uploadContext = context->getContext<FileUploadContext>()) {
                    LOG_INFO << "Cleaning up upload context for file: " << uploadContext->getFilename();
                }
            }
            conn->setContext(std::shared_ptr<void>());
        }
    }

    // 返回 true 表示同步处理完成，false 表示异步处理
    bool onRequest(const TcpConnectionPtr& conn, HttpRequest& req, HttpResponse* resp) {
        std::string path = req.path();
        LOG_INFO << "Headers " << req.methodString() << " " << path;
        LOG_INFO << "Content-Type: " << req.getHeader("Content-Type");
        LOG_INFO << "Body size: " << req.body().size();

        try {
            // 查找匹配的路由
            for (const auto& route : routes_) {
                if (route.method != req.method()) {
                    LOG_INFO << "Method mismatch: expected " << route.method << ", got " << req.method();
                    continue;
                }

                std::smatch matches;
                if (std::regex_match(path, matches, route.pattern)) {
                    LOG_INFO << "Found matching route: " << path;
                    // 提取路径参数
                    std::unordered_map<std::string, std::string> params;
                    for (size_t i = 0; i < route.params.size() && i + 1 < matches.size(); ++i) {
                        params[route.params[i]] = matches[i + 1];
                    }

                    // 将参数存储到请求对象中
                    req.setPathParams(params);

                    // 调用处理函数
                    return (this->*route.handler)(conn, req, resp);
                }
            }

            // 未找到匹配的路由，返回404
            LOG_WARN << "No matching route found for " << path;
            return handleNotFound(conn, resp);
        }
        catch (const std::exception& e) {
            LOG_ERROR << "Error processing request: " << e.what();
            sendError(resp, "Internal Server Error", HttpResponse::k500InternalServerError, conn);
            return true;
        }
    }

private:
    bool handleIndex(const TcpConnectionPtr& conn, HttpRequest& req, HttpResponse* resp) {
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("text/html; charset=utf-8");
        
        // 获取请求的路径
        std::string path = req.path();
        std::string filePath;
        
        LOG_INFO << "path = " << path;
        // 编译时获取当前文件目录
        std::string currentDir = __FILE__;
        std::string::size_type pos = currentDir.find_last_of("/");
        std::string projectRoot = currentDir.substr(0, pos);
        LOG_INFO << "projectRoot = " << projectRoot;
        // 根据请求路径选择不同的HTML文件
        if (path == "/register.html") {
            filePath = projectRoot + "/register.html";
        } else if (path == "/share.html" || path.find("/share/") == 0) {
            filePath = projectRoot + "/share.html";
        } else {
            filePath = projectRoot + "/index.html";
        }
        
        // 从文件中读取 HTML 内容
        std::ifstream file(filePath);
        if (!file.is_open()) {
            LOG_ERROR << "Failed to open " << filePath;
            sendError(resp, "Failed to open " + filePath, HttpResponse::k500InternalServerError, conn);
            return true;
        }
        
        std::string html((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        
        resp->addHeader("Connection", "close");
        resp->setBody(html);
        
        conn->setWriteCompleteCallback([](const TcpConnectionPtr& connection) {
            connection->shutdown();
            return true;
        });
        return true;
    }

    bool handleFileUpload(const TcpConnectionPtr& conn, HttpRequest& req, HttpResponse* resp) {
        // 验证会话
        std::string sessionId = req.getHeader("X-Session-ID");
        int userId;
        std::string usernameFromSession;
        
        if (!validateSession(sessionId, userId, usernameFromSession)) {
            sendError(resp, "未登录或会话已过期", HttpResponse::k401Unauthorized, conn);
            return true;
        }
        
        // 获取 HttpContext
        auto httpContext = std::static_pointer_cast<HttpContext>(conn->getContext());
        if (!httpContext) {
            LOG_ERROR << "HttpContext is null";
            sendError(resp, "Internal Server Error", HttpResponse::k500InternalServerError, conn);
            return true;
        }
        LOG_INFO << "body.size() = " << req.body().size();
        // 尝试获取已存在的上传上下文
        std::shared_ptr<FileUploadContext> uploadContext = httpContext->getContext<FileUploadContext>();

        if (!uploadContext) {
            // 解析 multipart/form-data 边界
            std::string contentType = req.getHeader("Content-Type");
            if (contentType.empty()) {
                sendError(resp, "Content-Type header is missing", HttpResponse::k400BadRequest, conn);
                return true;
            }
            
            std::regex boundaryRegex("boundary=(.+)$");
            std::smatch matches;
            if (!std::regex_search(contentType, matches, boundaryRegex)) {
                sendError(resp, "Invalid Content-Type", HttpResponse::k400BadRequest, conn);
                return true;
            }
            std::string boundary = "--" + matches[1].str();
            LOG_INFO << "Boundary: " << boundary;

            try {
                // 获取原始文件名
                std::string originalFilename;
                
                // 首先尝试从X-File-Name头部获取文件名
                std::string headerFilename = req.getHeader("X-File-Name");
                if (!headerFilename.empty()) {
                    originalFilename = urlDecode(headerFilename);
                    LOG_INFO << "Got filename from X-File-Name header: " << originalFilename;
                } else {
                    // 从Content-Disposition头中获取文件名
                    std::string body = req.body();
                    if (body.empty()) {
                        sendError(resp, "Request body is empty", HttpResponse::k400BadRequest, conn);
                        return true;
                    }
                    
                    std::regex filenameRegex("Content-Disposition:.*filename=\"([^\"]+)\"");
                    if (std::regex_search(body, matches, filenameRegex) && matches[1].matched) {
                        originalFilename = matches[1].str();
                        LOG_INFO << "Got filename from Content-Disposition: " << originalFilename;
                    } else {
                        originalFilename = "unknown_file";
                        LOG_INFO << "Using default filename: " << originalFilename;
                    }
                }

                // 生成服务器端文件名
                std::string filename = generateUniqueFilename("upload");
                std::string filepath = uploadDir_ + "/" + filename;
                
                // 创建上传上下文
                uploadContext = std::make_shared<FileUploadContext>(filepath, originalFilename);
                httpContext->setContext(uploadContext);
                
                // 设置边界
                uploadContext->setBoundary(boundary);
                
                // 解析body中的文件内容
                std::string body = req.body();
                size_t pos = body.find("\r\n\r\n");
                if (pos != std::string::npos) {
                    // 跳过头部信息，获取文件内容
                    pos += 4;
                    
                    // 检查是否是结束边界
                    std::string endBoundary = boundary + "--";
                    size_t endPos = body.find(endBoundary);
                    if (endPos != std::string::npos) {
                        LOG_INFO << "Found end boundary";
                        // 先写入边界之前的内容
                        if (endPos > 0) {
                            uploadContext->writeData(body.data() + pos, endPos - pos);
                            LOG_INFO << "Wrote " << endPos - pos << " bytes before end boundary, total: " << uploadContext->getTotalBytes();
                        }
                        // 找到结束边界，上传完成
                        uploadContext->setState(FileUploadContext::State::kComplete);
                    } else {
                        // 直接写入从pos开始的所有内容
                        if (pos < body.size()) {
                            uploadContext->writeData(body.data() + pos, body.size() - pos);
                            LOG_INFO << "Wrote " << body.size() - pos << " bytes, total: " << uploadContext->getTotalBytes();
                        }
                        uploadContext->setState(FileUploadContext::State::kExpectBoundary);
                    }
                }
                
                req.setBody(""); // 清空请求体
                LOG_INFO << "Created upload context for file: " << filepath;
            } catch (const std::exception& e) {
                LOG_ERROR << "Failed to create upload context: " << e.what();
                sendError(resp, "Failed to create file", HttpResponse::k500InternalServerError, conn);
                return true;
            }
        } else {
            try {
                // 处理后续的数据块
                std::string body = req.body();
                if (!body.empty()) {
                    LOG_INFO << "uploadContext->getState() = " << static_cast<int>(uploadContext->getState());
                    switch (uploadContext->getState()) {
                        case FileUploadContext::State::kExpectBoundary: {
                            // 检查是否是结束边界 格式为：--boundary--
                            std::string endBoundary = uploadContext->getBoundary() + "--";
                            size_t endPos = body.find(endBoundary);
                            if (endPos != std::string::npos) {
                                // 在这里添加写入边界之前的内容
                                if (endPos > 0) {
                                    uploadContext->writeData(body.data(), endPos);
                                    LOG_INFO << "Wrote " << endPos << " bytes before end boundary, total: " << uploadContext->getTotalBytes();
                                }
                                LOG_INFO << "Found end boundary";
                                // 找到结束边界，上传完成
                                uploadContext->setState(FileUploadContext::State::kComplete);
                                break;
                            }
                            // 检查是否是普通边界 格式为：--boundary
                            size_t boundaryPos = body.find(uploadContext->getBoundary());
                            LOG_INFO << "检查是否是普通边界 boundaryPos:" << boundaryPos;
                            if (boundaryPos != std::string::npos) {
                                // 找到新边界的开始，跳过边界和头部
                                size_t contentStart = body.find("\r\n\r\n", boundaryPos);
                                if (contentStart != std::string::npos) {
                                    contentStart += 4;
                                    // 写入边界之前的内容
                                    if (boundaryPos > 0) {
                                        uploadContext->writeData(body.data(), boundaryPos);
                                        LOG_INFO << "Wrote " << boundaryPos << " bytes, total: " << uploadContext->getTotalBytes();
                                    }
                                    // 更新状态
                                    uploadContext->setState(FileUploadContext::State::kExpectContent);
                                }
                            } else {
                                // 没有找到边界，写入所有内容
                                uploadContext->writeData(body.data(), body.size());
                                
                                LOG_INFO << "Wrote " << body.size() << " bytes, total: " << uploadContext->getTotalBytes();
                            }
                            break;
                        }
                        
                        case FileUploadContext::State::kExpectContent: {
                            // 检查是否包含下一个边界
                            size_t boundaryPos = body.find(uploadContext->getBoundary());
                            if (boundaryPos != std::string::npos) {
                                // 写入边界之前的内容
                                uploadContext->writeData(body.data(), boundaryPos);
                                LOG_INFO << "Wrote " << boundaryPos << " bytes, total: " << uploadContext->getTotalBytes();
                                // 更新状态
                                uploadContext->setState(FileUploadContext::State::kExpectBoundary);
                            } else {
                                // 没有找到边界，写入所有内容
                                uploadContext->writeData(body.data(), body.size());
                                LOG_INFO << "Wrote " << body.size() << " bytes, total: " << uploadContext->getTotalBytes();
                            }
                            break;
                        }
                        
                        case FileUploadContext::State::kComplete:
                            // 上传已完成，忽略后续数据
                            break;
                            
                        default:
                            LOG_INFO << "Unknown state: " << static_cast<int>(uploadContext->getState());
                            break;
                    }
                    
                    // 更新总处理字节数
                    // uploadContext->addProcessedBytes(body.size());
                }
            } catch (const std::exception& e) {
                LOG_ERROR << "Error processing data chunk: " << e.what();
                sendError(resp, "Failed to process data", HttpResponse::k500InternalServerError, conn);
                return true;
            }
        }
        req.setBody(""); // 清空请求体

        // 检查是否完成
        if (uploadContext->getState() == FileUploadContext::State::kComplete || httpContext->gotAll()) {
            // 上传完成，准备响应
            std::string serverFilename = fs::path(uploadContext->getFilename()).filename().string();
            std::string originalFilename = uploadContext->getOriginalFilename();
            uintmax_t fileSize = uploadContext->getTotalBytes();
            
            // 检测文件类型
            std::string fileType = getFileType(originalFilename);
            
            // 保存文件信息到数据库
            std::string query = "INSERT INTO files (filename, original_filename, file_size, file_type, user_id) VALUES ('" +
                              escapeString(serverFilename) + "', '" +
                              escapeString(originalFilename) + "', " +
                              std::to_string(fileSize) + ", '" +
                              escapeString(fileType) + "', " +
                              std::to_string(userId) + ")";
            
            if (!executeQuery(query)) {
                LOG_ERROR << "保存文件信息到数据库失败";
            }
            
            int fileId = static_cast<int>(mysql_insert_id(mysql));
            
            json response = {
                {"code", 0},
                {"message", "上传成功"},
                {"fileId", fileId},
                {"filename", serverFilename},
                {"originalFilename", originalFilename},
                {"size", fileSize}
            };

            resp->setStatusCode(HttpResponse::k200Ok);
            resp->setStatusMessage("OK");
            resp->setContentType("application/json");
            resp->addHeader("Connection", "close");
            resp->setBody(response.dump());

            // 清理上下文
            httpContext->setContext(nullptr);

            conn->setWriteCompleteCallback([](const TcpConnectionPtr& connection) {
                LOG_INFO << "Upload complete, closing connection";
                connection->shutdown();
                return true;
            });

            return true;
        } else {
            LOG_INFO << "Waiting for more data, current state: " 
                     << static_cast<int>(uploadContext->getState());
            return false;
        }
    }

    bool handleListFiles(const TcpConnectionPtr& conn, HttpRequest& req, HttpResponse* resp) {
        // 验证会话
        std::string sessionId = req.getHeader("X-Session-ID");
        int userId;
        std::string usernameFromSession;
        
        if (!validateSession(sessionId, userId, usernameFromSession)) {
            sendError(resp, "未登录或会话已过期", HttpResponse::k401Unauthorized, conn);
            return true;
        }
        
        // 获取文件列表类型，默认只显示自己的文件
        std::string listType = req.getQuery("type", "my");  // "my", "shared", "all"
        
        std::string query;
        if (listType == "my") {
            // 获取自己的文件selec
            query = "SELECT f.id, f.filename, f.original_filename, f.file_size, f.file_type, "
                    "f.created_at, 1 as is_owner FROM files f WHERE f.user_id = " + std::to_string(userId);
        } else if (listType == "shared") {
            // 获取分享给自己的文件
            query = "SELECT f.id, f.filename, f.original_filename, f.file_size, f.file_type, "
                    "f.created_at, 0 as is_owner FROM files f "
                    "JOIN file_shares fs ON f.id = fs.file_id "
                    "WHERE (fs.shared_with_id = " + std::to_string(userId) + " OR fs.share_type = 'public') "
                    "AND f.user_id != " + std::to_string(userId);
        } else if (listType == "all") {
            // 获取所有文件（包括自己的和分享的）
            query = "SELECT f.id, f.filename, f.original_filename, f.file_size, f.file_type, "
                    "f.created_at, CASE WHEN f.user_id = " + std::to_string(userId) + " THEN 1 ELSE 0 END as is_owner "
                    "FROM files f "
                    "LEFT JOIN file_shares fs ON f.id = fs.file_id "
                    "WHERE f.user_id = " + std::to_string(userId) + " OR "
                    "fs.shared_with_id = " + std::to_string(userId) + " OR fs.share_type = 'public'";
        }
        LOG_INFO << "query = " << query;
        MYSQL_RES* result = executeQueryWithResult(query);
        
        json response;
        response["code"] = 0;
        response["message"] = "Success";
        json files = json::array();
        
        if (result) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result))) {
                int fileId = std::stoi(row[0]);
                std::string filename = row[1];
                std::string originalFilename = row[2];
                uintmax_t fileSize = std::stoull(row[3]);
                std::string fileType = row[4];
                std::string createdAt = row[5];
                bool isOwner = (std::stoi(row[6]) == 1);
                
                // 如果是文件所有者，获取分享信息
                json shareInfo = nullptr;
                if (isOwner) {
                    std::string shareQuery = "SELECT share_type, shared_with_id, share_code, expire_time, extract_code FROM file_shares "
                                          "WHERE file_id = " + std::to_string(fileId);
                    MYSQL_RES* shareResult = executeQueryWithResult(shareQuery);
                    
                    if (shareResult && mysql_num_rows(shareResult) > 0) {
                        MYSQL_ROW shareRow = mysql_fetch_row(shareResult);
                        std::string shareType = shareRow[0];
                        
                        shareInfo = {
                            {"type", shareType},
                            {"shareCode", shareRow[2] ? shareRow[2] : ""}
                        };

                        if (shareType == "protected" && shareRow[4]) {
                            shareInfo["extractCode"] = shareRow[4];
                        }
                        
                        if (shareType == "user" && shareRow[1]) {
                            int sharedWithId = std::stoi(shareRow[1]);
                            // 获取共享用户的用户名
                            std::string userQuery = "SELECT username FROM users WHERE id = " + std::to_string(sharedWithId);
                            MYSQL_RES* userResult = executeQueryWithResult(userQuery);
                            if (userResult && mysql_num_rows(userResult) > 0) {
                                MYSQL_ROW userRow = mysql_fetch_row(userResult);
                                shareInfo["sharedWithUsername"] = userRow[0];
                                shareInfo["sharedWithId"] = sharedWithId;
                            }
                            if (userResult) mysql_free_result(userResult);
                        }
                        
                        if (shareRow[3]) {
                            shareInfo["expireTime"] = shareRow[3];
                        }
                    }
                    
                    if (shareResult) mysql_free_result(shareResult);
                }
                
                json fileInfo = {
                    {"id", fileId},
                    {"name", filename},
                    {"originalName", originalFilename},
                    {"size", fileSize},
                    {"type", fileType},
                    {"createdAt", createdAt},
                    {"isOwner", isOwner}
                };
                
                if (shareInfo != nullptr) {
                    fileInfo["shareInfo"] = shareInfo;
                }
                
                files.push_back(fileInfo);
            }
            response["files"] = files;
        }

        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("application/json");
        resp->addHeader("Connection", "close");
        resp->setBody(response.dump());

        // 设置写完成回调以关闭连接
        conn->setWriteCompleteCallback([](const TcpConnectionPtr& connection) {
            LOG_INFO << "List files complete, closing connection";
            connection->shutdown();
            return true;
        });

        return true;
    }

    bool handleDownload(const TcpConnectionPtr& conn, HttpRequest& req, HttpResponse* resp) {
        std::string filename = req.getPathParam("filename");
        if (filename.empty()) {
            sendError(resp, "Missing filename", HttpResponse::k400BadRequest, conn);
            return true;
        }
        
        // 检查用户是否已登录
        std::string sessionId = req.getHeader("X-Session-ID");
        // 如果请求头中没有sessionId，尝试从URL查询参数中获取
        if (sessionId.empty()) {
            sessionId = req.getQuery("sessionId", "");
            LOG_INFO << "从URL查询参数获取sessionId: " << sessionId;
        }
        
        int userId = 0;
        std::string usernameFromSession;
        bool isAuthenticated = validateSession(sessionId, userId, usernameFromSession);
        
        // 获取分享码(如果有)
        std::string shareCode = req.getQuery("code", "");
        std::string extractCode = req.getQuery("extract_code", "");
        LOG_INFO << "shareCode = " << shareCode << ", extractCode = " << extractCode;
        
        // 查询文件信息和权限
        std::string query;
        if (!shareCode.empty()) {
            // 通过分享链接访问
            query = "SELECT f.id, f.filename, f.original_filename, f.user_id, "
                    "fs.share_type, fs.shared_with_id, fs.extract_code "
                    "FROM files f "
                    "JOIN file_shares fs ON f.id = fs.file_id "
                    "WHERE f.filename = '" + escapeString(filename) + "' "
                    "AND fs.share_code = '" + escapeString(shareCode) + "' "
                    "AND (fs.expire_time IS NULL OR fs.expire_time > NOW())";
        } else {
            // 直接访问(需要登录)
            if (!isAuthenticated) {
                sendError(resp, "请先登录", HttpResponse::k401Unauthorized, conn);
                return true;
            }
            query = "SELECT f.id, f.filename, f.original_filename, f.user_id, "
                    "NULL as share_type, NULL as shared_with_id, NULL as extract_code "
                    "FROM files f WHERE f.filename = '" + escapeString(filename) + "'";
        }
        
        LOG_INFO << "查询文件信息: " << query;
        MYSQL_RES* result = executeQueryWithResult(query);
        
        if (!result || mysql_num_rows(result) == 0) {
            if (result) mysql_free_result(result);
            sendError(resp, "File not found", HttpResponse::k404NotFound, conn);
            return true;
        }
        
        MYSQL_ROW row = mysql_fetch_row(result);
        int fileId = std::stoi(row[0]);
        std::string serverFilename = row[1];
        std::string originalFilename = row[2];
        int fileOwnerId = std::stoi(row[3]);
        std::string shareType = row[4] ? row[4] : "";
        int sharedWithId = row[5] ? std::stoi(row[5]) : 0;
        std::string dbExtractCode = row[6] ? row[6] : "";
        mysql_free_result(result);
        
        // 检查访问权限
        bool hasPermission = false;
        
        if (isAuthenticated && userId == fileOwnerId) {
            // 文件所有者始终有权限
            hasPermission = true;
        } else if (!shareCode.empty()) {
            // 2. 通过分享链接访问
            if (shareType == "public") {
                // 完全公开
                LOG_INFO << "文件是完全公开的";
                hasPermission = true;
            } else if (shareType == "protected") {
                // 需要提取码
                LOG_INFO << "检查提取码: " << extractCode << " vs " << dbExtractCode;
                if (!extractCode.empty() && extractCode == dbExtractCode) {
                    hasPermission = true;
                }
            } else if (shareType == "user") {
                // 指定用户分享
                LOG_INFO << "检查用户权限: " << userId << " vs " << sharedWithId;
                if (isAuthenticated && userId == sharedWithId) {
                    hasPermission = true;
                }
            }
        } else if (isAuthenticated) {
            // 3. 直接访问(需要是所有者)
            hasPermission = (userId == fileOwnerId);
        }
        
        if (!hasPermission) {
            if (shareType == "protected" && (extractCode.empty() || extractCode != dbExtractCode)) {
                LOG_ERROR << "提取码错误或未提供";
                sendError(resp, "需要正确的提取码", HttpResponse::k403Forbidden, conn);
            } else {
                LOG_ERROR << "权限检查失败 - 用户ID: " << userId << ", 文件ID: " << fileId;
                sendError(resp, "您没有权限访问此文件", HttpResponse::k403Forbidden, conn);
            }
            return true;
        }
        
        LOG_INFO << "权限检查通过，准备下载文件";
        std::string filepath = uploadDir_ + "/" + serverFilename;
        
        try {
            if (!fs::exists(filepath) || !fs::is_regular_file(filepath)) {
                sendError(resp, "File not found", HttpResponse::k404NotFound, conn);
                return true;
            }
            
            // 获取文件大小
            uintmax_t fileSize = fs::file_size(filepath);
            
            // 如果是 HEAD 请求，只返回文件信息
            if (req.method() == HttpRequest::kHead) {
                resp->setStatusCode(HttpResponse::k200Ok);
                resp->setStatusMessage("OK");
                resp->setContentType("application/octet-stream");
                resp->addHeader("Content-Length", std::to_string(fileSize));
                resp->addHeader("Accept-Ranges", "bytes");
                resp->addHeader("Connection", "close");
                conn->setWriteCompleteCallback([](const TcpConnectionPtr& connection) {
                    connection->shutdown();
                    return true;
                });
                return true;
            }
            
            // 解析Range头部
            std::string rangeHeader = req.getHeader("Range");
            uintmax_t startPos = 0;
            uintmax_t endPos = fileSize - 1;
            bool isRangeRequest = false;
            
            if (!rangeHeader.empty()) {
                // 解析Range头部，格式为 "bytes=start-end"
                std::regex rangeRegex("bytes=(\\d+)-(\\d*)");
                std::smatch matches;
                if (std::regex_search(rangeHeader, matches, rangeRegex)) {
                    startPos = std::stoull(matches[1]);
                    if (!matches[2].str().empty()) {
                        endPos = std::stoull(matches[2]);
                    }
                    isRangeRequest = true;
                    
                    // 验证范围
                    if (startPos >= fileSize) {
                        sendError(resp, "Range Not Satisfiable", HttpResponse::k416RangeNotSatisfiable, conn);
                        return true;
                    }
                    
                    // 如果endPos未指定或超出文件大小，则使用文件大小-1
                    if (endPos >= fileSize) {
                        endPos = fileSize - 1;
                    }
                }
            }
            //打印 startPos ， endPos
            LOG_INFO << "startPos: " << startPos << ", endPos: " << endPos;
            
            // 获取 HttpContext
            auto httpContext = std::static_pointer_cast<HttpContext>(conn->getContext());
            if (!httpContext) {
                LOG_ERROR << "HttpContext is null";
                sendError(resp, "Internal Server Error", HttpResponse::k500InternalServerError, conn);
                return true;
            }
            
            // 尝试获取已存在的下载上下文
            std::shared_ptr<FileDownContext> downContext = httpContext->getContext<FileDownContext>();
            
            if (!downContext) {
                // 创建新的下载上下文
                downContext = std::make_shared<FileDownContext>(filepath, originalFilename);
                httpContext->setContext(downContext);
                
                // 设置初始响应头
                if (isRangeRequest) {
                    resp->setStatusCode(HttpResponse::k206PartialContent);
                    resp->setStatusMessage("Partial Content");
                    resp->addHeader("Content-Range", 
                                  "bytes " + std::to_string(startPos) + "-" + 
                                  std::to_string(endPos) + "/" + 
                                  std::to_string(fileSize));
                } else {
                    resp->setStatusCode(HttpResponse::k200Ok);
                    resp->setStatusMessage("OK");
                }
                
                resp->setContentType("application/octet-stream");
                resp->addHeader("Content-Disposition", 
                              "attachment; filename=\"" + originalFilename + "\"");
                resp->addHeader("Accept-Ranges", "bytes");
                resp->addHeader("Connection", "keep-alive");
                
                // 设置文件指针到起始位置
                downContext->seekTo(startPos);
            }
            
            // 设置写完成回调，继续发送下一个数据块
            conn->setWriteCompleteCallback([this, downContext](const TcpConnectionPtr& connection) {
                // 读取下一个数据块
                std::string chunk;
                if (downContext->readNextChunk(chunk)) {
                    // 直接发送数据块
                    connection->send(chunk);
                    return true;
                } else {
                    // 下载完成，关闭连接
                    LOG_INFO << "Download complete, closing connection";
                    connection->shutdown();
                    return true;
                }
            });
            
            return true;
        }
        catch (const std::exception& e) {
            LOG_ERROR << "Error during file download: " << e.what();
            sendError(resp, "Download failed", HttpResponse::k500InternalServerError, conn);
            return true;
        }
    }

    bool handleDelete(const TcpConnectionPtr& conn, HttpRequest& req, HttpResponse* resp) {
        // 验证会话
        std::string sessionId = req.getHeader("X-Session-ID");
        int userId;
        std::string usernameFromSession;
        
        if (!validateSession(sessionId, userId, usernameFromSession)) {
            sendError(resp, "未登录或会话已过期", HttpResponse::k401Unauthorized, conn);
            return true;
        }

        // 从路径参数中获取文件名
        std::string filename = req.getPathParam("filename");
        if (filename.empty()) {
            sendError(resp, "Missing filename", HttpResponse::k400BadRequest, conn);
            LOG_WARN << "Missing filename";
            return true;
        }
        
        // 构建完整的文件路径
        std::string filepath = uploadDir_ + "/" + filename;
        LOG_INFO << "filepath = " << filepath;

        // 检查文件所有权
        std::string query = "SELECT id FROM files WHERE filename = '" + escapeString(filename) + 
                          "' AND user_id = " + std::to_string(userId);
        MYSQL_RES* result = executeQueryWithResult(query);
        
        if (!result || mysql_num_rows(result) == 0) {
            if (result) mysql_free_result(result);
            sendError(resp, "文件不存在或您没有权限删除此文件", HttpResponse::k403Forbidden, conn);
            return true;
        }
        
        MYSQL_ROW row = mysql_fetch_row(result);
        int fileId = std::stoi(row[0]);
        mysql_free_result(result);
        
        // 删除文件分享记录
        std::string deleteSharesQuery = "DELETE FROM file_shares WHERE file_id = " + std::to_string(fileId);
        if (!executeQuery(deleteSharesQuery)) {
            LOG_ERROR << "删除文件分享记录失败";
        }
        
        // 删除文件记录
        std::string deleteFileQuery = "DELETE FROM files WHERE id = " + std::to_string(fileId);
        if (!executeQuery(deleteFileQuery)) {
            LOG_ERROR << "删除文件记录失败";
            sendError(resp, "删除文件记录失败", HttpResponse::k500InternalServerError, conn);
            return true;
        }
        
        // 检查文件是否存在
        if (access(filepath.c_str(), F_OK) != 0) {
            LOG_WARN << filepath << " not found";
        } else {
            // 删除文件
            if (unlink(filepath.c_str()) != 0) {
                LOG_WARN << "Failed to delete file: " << filepath << ", error: " << strerror(errno);
            }
            LOG_INFO << "delete file success";
        }
        
        // 删除文件名映射记录
        {
            std::lock_guard<std::mutex> lock(mappingMutex_);
            loadFilenameMappingInternal();  // 确保使用最新的映射
            filenameMapping_.erase(filename);
            saveFilenameMappingInternal();
        }

        // 删除成功，返回成功响应
        json response = {
            {"code", 0},
            {"message", "success"}
        };
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("application/json");
        resp->addHeader("Connection", "close");
        resp->setBody(response.dump());

        conn->setWriteCompleteCallback([](const TcpConnectionPtr& connection) {
            connection->shutdown();
            return true;
        });
        return true;
    }

    bool handleNotFound(const TcpConnectionPtr& conn, HttpResponse* resp) {
        json response = {
            {"code", 404},
            {"message", "Not Found"}
        };
        resp->setStatusCode(HttpResponse::k404NotFound);
        resp->setStatusMessage("Not Found");
        resp->setContentType("application/json");
        resp->addHeader("Connection", "close");
        resp->setBody(response.dump());

        conn->setWriteCompleteCallback([conn](const TcpConnectionPtr& connection) {
            connection->shutdown();
            return true;
        });
        return true;
    }

    static void sendError(HttpResponse* resp, const std::string& message, 
                  HttpResponse::HttpStatusCode code, const TcpConnectionPtr& conn) {
        json response = {
            {"code", static_cast<int>(code)},
            {"message", message}
        };
        resp->setStatusCode(code);
        resp->setStatusMessage(message);
        resp->setContentType("application/json");
        resp->addHeader("Connection", "close");
        resp->setBody(response.dump());

        if (conn) {
            conn->setWriteCompleteCallback([conn](const TcpConnectionPtr& connection) {
                connection->shutdown();
                return true;
            });
        }
    }

    std::string generateUniqueFilename(const std::string& prefix) {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1000, 9999);
        
        return prefix + "_" + std::to_string(timestamp) + "_" + 
               std::to_string(dis(gen));
    }

    // URL解码函数
    std::string urlDecode(const std::string& encoded) {
        std::string result;
        char ch;
        size_t i;
        int ii;
        size_t len = encoded.length();

        for (i = 0; i < len; i++) {
            if (encoded[i] != '%') {
                if (encoded[i] == '+')
                    result += ' ';
                else
                    result += encoded[i];
            } else {
                sscanf(encoded.substr(i + 1, 2).c_str(), "%x", &ii);
                ch = static_cast<char>(ii);
                result += ch;
                i = i + 2;
            }
        }
        return result;
    }

    // 加载文件名映射
    void loadFilenameMapping() {
        std::lock_guard<std::mutex> lock(mappingMutex_);
        loadFilenameMappingInternal();
    }

    // 保存文件名映射
    void saveFilenameMapping() {
        std::lock_guard<std::mutex> lock(mappingMutex_);
        saveFilenameMappingInternal();
    }

    // 内部函数，不加锁，调用方需要确保已经获得锁
    void loadFilenameMappingInternal() {
        try {
            if (fs::exists(mappingFile_)) {
                std::ifstream file(mappingFile_);
                filenameMapping_ = json::parse(file).get<std::map<std::string, std::string>>();
            }
        } catch (const std::exception& e) {
            LOG_ERROR << "Failed to load filename mapping: " << e.what();
        }
    }

    // 内部函数，不加锁，调用方需要确保已经获得锁
    void saveFilenameMappingInternal() {
        try {
            std::ofstream file(mappingFile_);
            file << json(filenameMapping_).dump(2);
        } catch (const std::exception& e) {
            LOG_ERROR << "Failed to save filename mapping: " << e.what();
        }
    }

    // 外部接口，带锁保护
    void addFilenameMapping(const std::string& serverFilename, const std::string& originalFilename) {
        std::lock_guard<std::mutex> lock(mappingMutex_);
        loadFilenameMappingInternal();  // 先加载最新的映射
        filenameMapping_[serverFilename] = originalFilename;
        saveFilenameMappingInternal();
    }

    // 外部接口，带锁保护
    std::string getOriginalFilename(const std::string& serverFilename) {
        std::lock_guard<std::mutex> lock(mappingMutex_);
        loadFilenameMappingInternal();  // 确保使用最新的映射
        auto it = filenameMapping_.find(serverFilename);
        return it != filenameMapping_.end() ? it->second : serverFilename;
    }

    // 外部接口，带锁保护
    std::map<std::string, std::string> getAllFileMappings() {
        std::lock_guard<std::mutex> lock(mappingMutex_);
        loadFilenameMappingInternal();
        return filenameMapping_;
    }

    // 用户注册
    bool handleRegister(const TcpConnectionPtr& conn, HttpRequest& req, HttpResponse* resp) {
        LOG_INFO << "Handling register request";
        LOG_INFO << "Request body: " << req.body();
        
        try {
            json requestData = json::parse(req.body());
            std::string username = requestData["username"];
            std::string password = requestData["password"];
            std::string email = requestData.value("email", "");

            LOG_INFO << "Register attempt for username: " << username;
            // 简单验证
            if (username.empty() || password.empty()) {
                sendError(resp, "用户名和密码不能为空", HttpResponse::k400BadRequest, conn);
                return true;
            }

            // 哈希密码
            // 在实际应用中应使用更安全的哈希算法，这里简化处理
            std::string hashedPassword = sha256(password);

            // 检查用户名是否已存在
            std::string checkQuery = "SELECT id FROM users WHERE username = '" + 
                                   escapeString(username) + "'";
            MYSQL_RES* result = executeQueryWithResult(checkQuery);
            
            if (result && mysql_num_rows(result) > 0) {
                mysql_free_result(result);
                sendError(resp, "用户名已存在", HttpResponse::k400BadRequest, conn);
                return true;
            }
            
            if (result) {
                mysql_free_result(result);
            }

            // 插入新用户
            std::string insertQuery = "INSERT INTO users (username, password, email) VALUES ('" +
                                    escapeString(username) + "', '" +
                                    escapeString(hashedPassword) + "', " +
                                    (email.empty() ? "NULL" : ("'" + escapeString(email) + "'")) + ")";
            
            if (!executeQuery(insertQuery)) {
                sendError(resp, "注册失败，请稍后重试", HttpResponse::k500InternalServerError, conn);
                return true;
            }

            // 获取新用户ID
            int userId = static_cast<int>(mysql_insert_id(mysql));

            json response = {
                {"code", 0},
                {"message", "注册成功"},
                {"userId", userId}
            };

            resp->setStatusCode(HttpResponse::k200Ok);
            resp->setStatusMessage("OK");
            resp->setContentType("application/json");
            resp->addHeader("Connection", "close");
            resp->setBody(response.dump());

            conn->setWriteCompleteCallback([](const TcpConnectionPtr& connection) {
                connection->shutdown();
                return true;
            });

            return true;
        }
        catch (const std::exception& e) {
            LOG_ERROR << "用户注册错误: " << e.what();
            sendError(resp, "注册失败: " + std::string(e.what()), HttpResponse::k500InternalServerError, conn);
            return true;
        }
    }

    // 用户登录
    bool handleLogin(const TcpConnectionPtr& conn, HttpRequest& req, HttpResponse* resp) {
        try {
            json requestData = json::parse(req.body());
            std::string username = requestData["username"];
            std::string password = requestData["password"];

            // 验证参数
            if (username.empty() || password.empty()) {
                sendError(resp, "用户名和密码不能为空", HttpResponse::k400BadRequest, conn);
                return true;
            }

            // 哈希密码
            std::string hashedPassword = sha256(password);

            // 查询用户
            std::string query = "SELECT id, username FROM users WHERE username = '" +
                              escapeString(username) + "' AND password = '" +
                              escapeString(hashedPassword) + "'";
            
            MYSQL_RES* result = executeQueryWithResult(query);
            
            if (!result || mysql_num_rows(result) == 0) {
                if (result) {
                    mysql_free_result(result);
                }
                sendError(resp, "用户名或密码错误", HttpResponse::k401Unauthorized, conn);
                return true;
            }

            MYSQL_ROW row = mysql_fetch_row(result);
            int userId = std::stoi(row[0]);
            std::string usernameFromDb = row[1];
            mysql_free_result(result);

            // 创建会话
            std::string sessionId = generateSessionId();
            saveSession(sessionId, userId, usernameFromDb);

            json response = {
                {"code", 0},
                {"message", "登录成功"},
                {"sessionId", sessionId},
                {"userId", userId},
                {"username", usernameFromDb}
            };

            resp->setStatusCode(HttpResponse::k200Ok);
            resp->setStatusMessage("OK");
            resp->setContentType("application/json");
            resp->addHeader("Connection", "close");
            resp->setBody(response.dump());

            conn->setWriteCompleteCallback([](const TcpConnectionPtr& connection) {
                connection->shutdown();
                return true;
            });

            return true;
        }
        catch (const std::exception& e) {
            LOG_ERROR << "用户登录错误: " << e.what();
            sendError(resp, "登录失败: " + std::string(e.what()), HttpResponse::k500InternalServerError, conn);
            return true;
        }
    }

    // 会话管理
    private:
        // 生成会话ID
        std::string generateSessionId() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 35);
            
            const char* chars = "abcdefghijklmnopqrstuvwxyz0123456789";
            std::string sessionId;
            sessionId.reserve(32);
            
            for (int i = 0; i < 32; ++i) {
                sessionId += chars[dis(gen)];
            }
            
            return sessionId;
        }
        
        // 保存会话
        void saveSession(const std::string& sessionId, int userId, const std::string& username) {
            std::string query = "INSERT INTO sessions (session_id, user_id, username, expire_time) VALUES ('" +
                              escapeString(sessionId) + "', " +
                              std::to_string(userId) + ", '" +
                              escapeString(username) + "', " +
                              "DATE_ADD(NOW(), INTERVAL 30 MINUTE))";
            
            executeQuery(query);
        }
        
        // 验证会话
        bool validateSession(const std::string& sessionId, int& userId, std::string& username) {
            if (sessionId.empty()) {
                LOG_WARN << "sessionId is empty";
                return false;
            }
            
            std::string query = "SELECT user_id, username FROM sessions WHERE session_id = '" +
                              escapeString(sessionId) + "' AND expire_time > NOW()";
            
            MYSQL_RES* result = executeQueryWithResult(query);
            
            if (!result || mysql_num_rows(result) == 0) {
                if (result) {
                    mysql_free_result(result);
                }
                LOG_WARN << "mysql_num_rows is invalid";
                return false;
            }
            
            MYSQL_ROW row = mysql_fetch_row(result);
            userId = std::stoi(row[0]);
            username = row[1];
            mysql_free_result(result);
            
            // 更新会话过期时间
            std::string updateQuery = "UPDATE sessions SET expire_time = DATE_ADD(NOW(), INTERVAL 30 MINUTE) WHERE session_id = '" +
                                    escapeString(sessionId) + "'";
            executeQuery(updateQuery);
            LOG_INFO << "validateSession success";
            return true;
        }
        
        // 结束会话
        void endSession(const std::string& sessionId) {
            if (sessionId.empty()) {
                return;
            }
            
            std::string query = "DELETE FROM sessions WHERE session_id = '" +
                              escapeString(sessionId) + "'";
            
            executeQuery(query);
        }
        
        // 转义SQL字符串
        std::string escapeString(const std::string& str) {
            if (!mysql) {
                return str;
            }
            
            char* escaped = new char[str.length() * 2 + 1];
            mysql_real_escape_string(mysql, escaped, str.c_str(), str.length());
            std::string result(escaped);
            delete[] escaped;
            
            return result;
        }

    // 获取文件类型
    std::string getFileType(const std::string& filename) {
        size_t dotPos = filename.find_last_of('.');
        if (dotPos != std::string::npos && dotPos < filename.length() - 1) {
            std::string extension = filename.substr(dotPos + 1);
            std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
            
            // 根据扩展名判断文件类型
            if (extension == "jpg" || extension == "jpeg" || extension == "png" || extension == "gif") {
                return "image";
            } else if (extension == "mp4" || extension == "avi" || extension == "mov" || extension == "wmv") {
                return "video";
            } else if (extension == "pdf") {
                return "pdf";
            } else if (extension == "doc" || extension == "docx") {
                return "word";
            } else if (extension == "xls" || extension == "xlsx") {
                return "excel";
            } else if (extension == "ppt" || extension == "pptx") {
                return "powerpoint";
            } else if (extension == "txt" || extension == "csv") {
                return "text";
            } else {
                return "other";
            }
        }
        return "unknown";
    }

    void initRoutes() {
        // 不需要会话验证的路由
        addRoute("/favicon.ico", HttpRequest::kGet, &HttpUploadHandler::handleFavicon);
        addRoute("/register", HttpRequest::kPost, &HttpUploadHandler::handleRegister);
        addRoute("/login", HttpRequest::kPost, &HttpUploadHandler::handleLogin);
        addRoute("/", HttpRequest::kGet, &HttpUploadHandler::handleIndex);
        addRoute("/index.html", HttpRequest::kGet, &HttpUploadHandler::handleIndex);
        addRoute("/register.html", HttpRequest::kGet, &HttpUploadHandler::handleIndex);
        addRoute("/share/([^/]+)", HttpRequest::kGet, &HttpUploadHandler::handleShareAccess, {"code"});
        addRoute("/share/download/([^/]+)", HttpRequest::kGet, &HttpUploadHandler::handleShareDownload, {"filename"});
        addRoute("/share/info/([^/]+)", HttpRequest::kGet, &HttpUploadHandler::handleShareInfo, {"code"});
        
        // 需要会话验证的路由
        addRoute("/upload", HttpRequest::kPost, &HttpUploadHandler::handleFileUpload);
        addRoute("/files", HttpRequest::kGet, &HttpUploadHandler::handleListFiles);
        addRoute("/download/([^/]+)", HttpRequest::kHead, &HttpUploadHandler::handleDownload, {"filename"});

        addRoute("/download/([^/]+)", HttpRequest::kGet, &HttpUploadHandler::handleDownload, {"filename"});
        addRoute("/delete/([^/]+)", HttpRequest::kDelete, &HttpUploadHandler::handleDelete, {"filename"});
        addRoute("/share", HttpRequest::kPost, &HttpUploadHandler::handleShareFile);
        addRoute("/users/search", HttpRequest::kGet, &HttpUploadHandler::handleSearchUsers);
        addRoute("/logout", HttpRequest::kPost, &HttpUploadHandler::handleLogout);
    }

    // 添加精确匹配的路由
    void addRoute(const std::string& path, HttpRequest::Method method, RequestHandler handler) {
        std::string pattern = "^" + escapeRegex(path) + "$";
        routes_.emplace_back(pattern, std::vector<std::string>(), handler, method);
    }

    // 添加带参数的路由
    void addRoute(const std::string& pattern, HttpRequest::Method method, 
                 RequestHandler handler, const std::vector<std::string>& paramNames) {
        routes_.emplace_back(pattern, paramNames, handler, method);
    }

    // 转义正则表达式特殊字符
    std::string escapeRegex(const std::string& str) {
        std::string result;
        for (char c : str) {
            if (c == '.' || c == '+' || c == '*' || c == '?' || c == '^' || 
                c == '$' || c == '(' || c == ')' || c == '[' || c == ']' || 
                c == '{' || c == '}' || c == '|' || c == '\\') {
                result += '\\';
            }
            result += c;
        }
        return result;
    }

    // SHA256 哈希算法简单实现
    std::string sha256(const std::string& input) {
        // 这只是一个简单的哈希表示，不是真正的SHA256
        // 在生产环境中，应使用专业的密码学库
        std::hash<std::string> hasher;
        auto hash = hasher(input);
        std::stringstream ss;
        ss << std::hex << hash;
        return ss.str();
    }

    // 登出处理
    bool handleLogout(const TcpConnectionPtr& conn, HttpRequest& req, HttpResponse* resp) {
        std::string sessionId = req.getHeader("X-Session-ID");
        if (!sessionId.empty()) {
            endSession(sessionId);
        }

        json response = {
            {"code", 0},
            {"message", "Logout successful"}
        };

        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("application/json");
        resp->addHeader("Connection", "close");
        resp->setBody(response.dump());

        conn->setWriteCompleteCallback([](const TcpConnectionPtr& connection) {
            connection->shutdown();
            return true;
        });
        return true;
    }

    // 搜索用户
    bool handleSearchUsers(const TcpConnectionPtr& conn, HttpRequest& req, HttpResponse* resp) {
        // 验证会话
        std::string sessionId = req.getHeader("X-Session-ID");
        int userId;
        std::string username;
        
        if (!validateSession(sessionId, userId, username)) {
            sendError(resp, "未登录或会话已过期", HttpResponse::k401Unauthorized, conn);
            LOG_WARN << "validateSession failed";
            return true;
        }
        
        // 从查询参数获取关键词
        std::string query = req.query();
        LOG_INFO << "query = " << query;
        std::string keyword;

        // 如果查询字符串以 ? 开头，去掉它
        if (!query.empty() && query[0] == '?') {
            query = query.substr(1);
        }
        
        if (query.find("keyword=") == 0) {
            keyword = urlDecode(query.substr(8));
        } else {
            sendError(resp, "搜索关键词不能为空", HttpResponse::k400BadRequest, conn);
            LOG_WARN << "keyword is empty";
            return true;
        }
        
        if (keyword.empty()) {
            sendError(resp, "搜索关键词不能为空", HttpResponse::k400BadRequest, conn);
            LOG_WARN << "keyword is empty";
            return true;
        }
        
        std::string sqlQuery = "SELECT id, username, email FROM users WHERE username LIKE '%" + 
                            escapeString(keyword) + "%' AND id != " + std::to_string(userId) + 
                            " LIMIT 10";
        LOG_INFO << "sqlQuery = " << sqlQuery;
        MYSQL_RES* result = executeQueryWithResult(sqlQuery);
        
        json response;
        response["code"] = 0;
        response["message"] = "Success";
        json users = json::array();
        
        if (result) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result))) {
                int id = std::stoi(row[0]);
                std::string usernameFromDb = row[1] ? row[1] : "";
                std::string email = row[2] ? row[2] : "";
                
                users.push_back({
                    {"id", id},
                    {"username", usernameFromDb},
                    {"email", email}
                });
            }
            
            mysql_free_result(result);
        }
        
        response["users"] = users;
        
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("application/json");
        resp->addHeader("Connection", "close");
        resp->setBody(response.dump());
        LOG_INFO << "response = " << response.dump();
        conn->setWriteCompleteCallback([](const TcpConnectionPtr& connection) {
            connection->shutdown();
            return true;
        });
        
        return true;
    }

    // 生成分享码(用于URL)
    std::string generateShareCode() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 35);
        
        const char* chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        std::string shareCode;
        shareCode.reserve(32);
        
        for (int i = 0; i < 32; ++i) {
            shareCode += chars[dis(gen)];
        }
        
        return shareCode;
    }

    // 生成提取码
    std::string generateExtractCode() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 35);
        
        const char* chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        std::string extractCode;
        extractCode.reserve(6);
        
        for (int i = 0; i < 6; ++i) {
            extractCode += chars[dis(gen)];
        }
        
        return extractCode;
    }

    // 文件分享处理函数
    bool handleShareFile(const TcpConnectionPtr& conn, HttpRequest& req, HttpResponse* resp) {
        // 验证会话
        std::string sessionId = req.getHeader("X-Session-ID");
        int userId;
        std::string usernameFromSession;
        
        if (!validateSession(sessionId, userId, usernameFromSession)) {
            sendError(resp, "未登录或会话已过期", HttpResponse::k401Unauthorized, conn);
            return true;
        }
        
        try {
            json requestData = json::parse(req.body());
            int fileId = requestData["fileId"];
            std::string shareType = requestData["shareType"];  // "private", "public", "protected", "user"
            
            // 验证文件所有权
            std::string fileQuery = "SELECT 1 FROM files WHERE id = " + std::to_string(fileId) + 
                                " AND user_id = " + std::to_string(userId);
            MYSQL_RES* fileResult = executeQueryWithResult(fileQuery);
            
            if (!fileResult || mysql_num_rows(fileResult) == 0) {
                if (fileResult) mysql_free_result(fileResult);
                sendError(resp, "您没有权限分享此文件", HttpResponse::k403Forbidden, conn);
                return true;
            }
            
            if (fileResult) mysql_free_result(fileResult);
            
            // 如果是私有文件，删除所有分享记录
            if (shareType == "private") {
                std::string deleteQuery = "DELETE FROM file_shares WHERE file_id = " + std::to_string(fileId);
                executeQuery(deleteQuery);
                
                json response = {
                    {"code", 0},
                    {"message", "文件设置为私有成功"}
                };
                
                resp->setStatusCode(HttpResponse::k200Ok);
                resp->setStatusMessage("OK");
                resp->setContentType("application/json");
                resp->addHeader("Connection", "close");
                resp->setBody(response.dump());
                
                conn->setWriteCompleteCallback([](const TcpConnectionPtr& connection) {
                    connection->shutdown();
                    return true;
                });
                
                return true;
            }
            
            // 处理到期时间
            std::string expireStr = "NULL";
            if (requestData.contains("expireTime") && !requestData["expireTime"].is_null()) {
                int expireHours = requestData["expireTime"];
                if (expireHours > 0) {
                    expireStr = "DATE_ADD(NOW(), INTERVAL " + std::to_string(expireHours) + " HOUR)";
                }
            }
            
            // 生成分享码(用于URL)
            std::string shareCode = generateShareCode();
            
            // 处理分享类型
            std::string sharedWithId = "NULL";
            std::string extractCode = "NULL";
            
            if (shareType == "user" && requestData.contains("sharedWithId")) {
                // 指定用户分享
                sharedWithId = std::to_string(requestData["sharedWithId"].get<int>());
                // 检查是否已经分享给该用户
                std::string checkQuery = "SELECT 1 FROM file_shares WHERE file_id = " + std::to_string(fileId) + 
                                      " AND shared_with_id = " + sharedWithId + 
                                      " AND share_type = 'user'";
                MYSQL_RES* checkResult = executeQueryWithResult(checkQuery);
                if (checkResult && mysql_num_rows(checkResult) > 0) {
                    if (checkResult) mysql_free_result(checkResult);
                    sendError(resp, "已经分享给该用户", HttpResponse::k400BadRequest, conn);
                    return true;
                }
                if (checkResult) mysql_free_result(checkResult);
            } else if (shareType == "protected") {
                // 生成提取码
                extractCode = "'" + generateExtractCode() + "'";
            }
            
            // 创建分享记录
            std::string insertQuery = "INSERT INTO file_shares (file_id, owner_id, shared_with_id, share_type, share_code, extract_code, expire_time) VALUES (" +
                                    std::to_string(fileId) + ", " +
                                    std::to_string(userId) + ", " +
                                    sharedWithId + ", '" +
                                    escapeString(shareType) + "', '" +
                                    escapeString(shareCode) + "', " +
                                    extractCode + ", " +
                                    expireStr + ")";
            
            if (!executeQuery(insertQuery)) {
                sendError(resp, "创建分享失败", HttpResponse::k500InternalServerError, conn);
                return true;
            }
            
            // 获取新创建的分享记录ID
            int shareId = static_cast<int>(mysql_insert_id(mysql));
            
            json response = {
                {"code", 0},
                {"message", "分享成功"},
                {"shareId", shareId},
                {"shareType", shareType},
                {"shareCode", shareCode},
                {"shareLink", "/share/" + shareCode}
            };
            
            if (shareType == "user") {
                response["sharedWithId"] = std::stoi(sharedWithId);
            } else if (shareType == "protected") {
                response["extractCode"] = extractCode.substr(1, extractCode.length() - 2);  // 去掉引号
            }
            
            resp->setStatusCode(HttpResponse::k200Ok);
            resp->setStatusMessage("OK");
            resp->setContentType("application/json");
            resp->addHeader("Connection", "close");
            resp->setBody(response.dump());
            
            conn->setWriteCompleteCallback([](const TcpConnectionPtr& connection) {
                connection->shutdown();
                return true;
            });
            
            return true;
        }
        catch (const std::exception& e) {
            LOG_ERROR << "分享文件错误: " << e.what();
            sendError(resp, "分享失败: " + std::string(e.what()), HttpResponse::k500InternalServerError, conn);
            return true;
        }
    }

    // 通过分享码访问文件
    bool handleShareAccess(const TcpConnectionPtr& conn, HttpRequest& req, HttpResponse* resp) {
        std::string path = req.path();
        std::smatch matches;
        std::regex codeRegex("/share/([^/]+)");
        LOG_INFO << "path = " << path;
        
        if (!std::regex_search(path, matches, codeRegex) || matches.size() < 2) {
            sendError(resp, "无效的分享链接", HttpResponse::k400BadRequest, conn);
            LOG_WARN << "invalid share link";
            return true;
        }

        std::string shareCode = matches[1];
        std::string acceptHeader = req.getHeader("Accept");
        
        // 检查用户是否已登录
        std::string sessionId = req.getHeader("X-Session-ID");
        int userId = 0;
        std::string username;
        bool isAuthenticated = validateSession(sessionId, userId, username);
        
        // 如果是AJAX请求（请求JSON数据），返回文件信息
        if (req.getHeader("X-Requested-With") == "XMLHttpRequest" || 
            acceptHeader.find("application/json") != std::string::npos) {
            LOG_INFO << "AJAX请求，返回文件信息, shareCode = " << shareCode;
            
            // 检查分享码格式
            if (shareCode.empty() || shareCode.length() != 32) {
                sendError(resp, "无效的分享码格式", HttpResponse::k400BadRequest, conn);
                return true;
            }
            
            // 检查分享码是否包含非法字符
            if (!std::all_of(shareCode.begin(), shareCode.end(), [](char c) {
                return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
            })) {
                sendError(resp, "分享码包含非法字符", HttpResponse::k400BadRequest, conn);
                return true;
            }

            // 获取提取码(如果有)
            std::string extractCode = req.getQuery("code", "");
            
            // 构建查询语句
            std::string query = "SELECT fs.*, f.filename, f.original_filename, f.file_size, "
                              "f.file_type, u.username as owner_username, f.user_id "
                              "FROM file_shares fs "
                              "JOIN files f ON fs.file_id = f.id "
                              "JOIN users u ON f.user_id = u.id "
                              "WHERE fs.share_code = '" + escapeString(shareCode) + "' "
                              "AND (fs.expire_time IS NULL OR fs.expire_time > NOW()) "
                              "AND (fs.share_type != 'protected' OR (fs.share_type = 'protected' AND fs.extract_code = '" + escapeString(extractCode) + "'))";
            
            LOG_INFO << "query = " << query;
            MYSQL_RES* result = executeQueryWithResult(query);
            
            if (!result || mysql_num_rows(result) == 0) {
                if (result) mysql_free_result(result);
                sendError(resp, "分享链接已失效或不存在", HttpResponse::k404NotFound, conn);
                return true;
            }

            MYSQL_ROW row = mysql_fetch_row(result);
            std::string shareType = row[4];  // fs.share_type
            bool isOwner = (row[13] && std::stoi(row[13]) == userId);  // f.user_id == current_user_id
            int sharedWithId = row[3] ? std::stoi(row[3]) : 0;  // fs.shared_with_id
            std::string dbExtractCode = row[5] ? row[5] : "";  // fs.extract_code
            
            // 检查访问权限
            bool hasPermission = false;
            
            if (isOwner) {
                // 文件所有者始终有权限
                hasPermission = true;
            } else if (shareType == "public") {
                // 完全公开，任何人都可以访问
                hasPermission = true;
            } else if (shareType == "protected") {
                // 需要提取码
                if (!extractCode.empty() && extractCode == dbExtractCode) {
                    hasPermission = true;
                }
            } else if (shareType == "user") {
                // 指定用户且已登录
                if (isAuthenticated && userId == sharedWithId) {
                    hasPermission = true;
                }
            }
            
            if (!hasPermission) {
                mysql_free_result(result);
                if (shareType == "protected" && (extractCode.empty() || extractCode != dbExtractCode)) {
                    sendError(resp, "需要正确的提取码", HttpResponse::k403Forbidden, conn);
                } else {
                    sendError(resp, "您没有权限访问此文件", HttpResponse::k403Forbidden, conn);
                }
                return true;
            }

            json response = {
                {"code", 0},
                {"message", "success"},
                {"file", {
                    {"id", std::stoi(row[0])},              // fs.id
                    {"fileId", std::stoi(row[1])},          // fs.file_id
                    {"ownerId", std::stoi(row[2])},         // fs.owner_id
                    {"sharedWithId", row[3] ? std::stoi(row[3]) : 0}, // fs.shared_with_id
                    {"shareType", shareType},               // fs.share_type
                    {"shareCode", shareCode},               // fs.share_code
                    {"createdAt", row[6] ? row[6] : ""},   // fs.created_at
                    {"expireTime", row[7] ? row[7] : ""},   // fs.expire_time
                    {"filename", row[8] ? row[8] : ""},     // f.filename
                    {"originalName", row[9] ? row[9] : ""}, // f.original_filename
                    {"size", row[10] ? std::stoull(row[10]) : 0}, // f.file_size
                    {"type", row[11] ? row[11] : "unknown"}, // f.file_type
                    {"ownerUsername", row[12] ? row[12] : ""}, // u.username
                    {"isOwner", isOwner}
                }},
                {"downloadUrl", "/share/download/" + std::string(row[8] ? row[8] : "") + "?code=" + shareCode}
            };

            mysql_free_result(result);

            resp->setStatusCode(HttpResponse::k200Ok);
            resp->setStatusMessage("OK");
            resp->setContentType("application/json");
            resp->setBody(response.dump());
            
        } else {
            LOG_INFO << "返回分享页面 share.html";
            // 返回share.html页面，并在响应头中添加分享码
            bool result = handleIndex(conn, req, resp);
            resp->addHeader("X-Share-Code", shareCode);
            return result;
        }

        resp->addHeader("Connection", "close");
        conn->setWriteCompleteCallback([](const TcpConnectionPtr& connection) {
            connection->shutdown();
            return true;
        });
        
        return true;
    }

    // 处理通过分享链接下载文件
    bool handleShareDownload(const TcpConnectionPtr& conn, HttpRequest& req, HttpResponse* resp) {
        std::string filename = req.getPathParam("filename");
        if (filename.empty()) {
            sendError(resp, "Missing filename", HttpResponse::k400BadRequest, conn);
            return true;
        }
        
        // 获取分享码和提取码
        std::string shareCode = req.getQuery("code", "");
        std::string extractCode = req.getQuery("extract_code", "");
        LOG_INFO << "shareCode = " << shareCode << ", extractCode = " << extractCode;
        if (shareCode.empty()) {
            sendError(resp, "Missing share code", HttpResponse::k400BadRequest, conn);
            return true;
        }
        
        // 检查用户是否已登录（可选）
        std::string sessionId = req.getHeader("X-Session-ID");
        int userId = 0;
        std::string usernameFromSession;
        bool isAuthenticated = validateSession(sessionId, userId, usernameFromSession);
        
        // 查询分享信息
        std::string query = "SELECT f.id, f.filename, f.original_filename, f.user_id, "
                          "fs.share_type, fs.shared_with_id, fs.extract_code, "
                          "fs.created_at, fs.expire_time "
                          "FROM files f "
                          "JOIN file_shares fs ON f.id = fs.file_id "
                          "WHERE f.filename = '" + escapeString(filename) + "' "
                          "AND fs.share_code = '" + escapeString(shareCode) + "' "
                          "AND (fs.expire_time IS NULL OR fs.expire_time > NOW())";
        
        LOG_INFO << "查询分享信息: " << query;
        MYSQL_RES* result = executeQueryWithResult(query);
        
        if (!result || mysql_num_rows(result) == 0) {
            if (result) mysql_free_result(result);
            LOG_ERROR << "分享不存在或已过期";
            sendError(resp, "Share not found or expired", HttpResponse::k404NotFound, conn);
            return true;
        }
        
        MYSQL_ROW row = mysql_fetch_row(result);
        // int fileId = std::stoi(row[0]);
        std::string serverFilename = row[1];
        std::string originalFilename = row[2];
        int fileOwnerId = std::stoi(row[3]);
        std::string shareType = row[4];
        int sharedWithId = row[5] ? std::stoi(row[5]) : 0;
        std::string dbExtractCode = row[6] ? row[6] : "";
        std::string createdAt = row[7] ? row[7] : "";
        std::string expireTime = row[8] ? row[8] : "";
        mysql_free_result(result);
        
        // 检查访问权限
        bool hasPermission = false;
        
        if (isAuthenticated && userId == fileOwnerId) {
            // 文件所有者始终有权限
            hasPermission = true;
        } else if (shareType == "public") {
            // 完全公开
            hasPermission = true;
        } else if (shareType == "protected") {
            // 需要提取码
            if (!extractCode.empty() && extractCode == dbExtractCode) {
                hasPermission = true;
            }
        } else if (shareType == "user") {
            // 指定用户且已登录
            if (isAuthenticated && userId == sharedWithId) {
                hasPermission = true;
            }
        }
        
        if (!hasPermission) {
            if (shareType == "protected" && (extractCode.empty() || extractCode != dbExtractCode)) {
                sendError(resp, "需要正确的提取码", HttpResponse::k403Forbidden, conn);
            } else {
                sendError(resp, "您没有权限访问此文件", HttpResponse::k403Forbidden, conn);
            }
            return true;
        }
        
        // 开始下载文件
        std::string filepath = uploadDir_ + "/" + serverFilename;
        
        try {
            if (!fs::exists(filepath) || !fs::is_regular_file(filepath)) {
                sendError(resp, "File not found", HttpResponse::k404NotFound, conn);
                return true;
            }
            
            // 获取文件大小
            uintmax_t fileSize = fs::file_size(filepath);
            
            // 如果是 HEAD 请求，只返回文件信息
            if (req.method() == HttpRequest::kHead) {
                resp->setStatusCode(HttpResponse::k200Ok);
                resp->setStatusMessage("OK");
                resp->setContentType("application/octet-stream");
                resp->addHeader("Content-Length", std::to_string(fileSize));
                resp->addHeader("Accept-Ranges", "bytes");
                resp->addHeader("Connection", "close");
                conn->setWriteCompleteCallback([](const TcpConnectionPtr& connection) {
                    connection->shutdown();
                    return true;
                });
                return true;
            }
            
            // 解析Range头部
            std::string rangeHeader = req.getHeader("Range");
            uintmax_t startPos = 0;
            uintmax_t endPos = fileSize - 1;
            bool isRangeRequest = false;
            
            if (!rangeHeader.empty()) {
                // 解析Range头部，格式为 "bytes=start-end"
                std::regex rangeRegex("bytes=(\\d+)-(\\d*)");
                std::smatch matches;
                if (std::regex_search(rangeHeader, matches, rangeRegex)) {
                    startPos = std::stoull(matches[1]);
                    if (!matches[2].str().empty()) {
                        endPos = std::stoull(matches[2]);
                    }
                    isRangeRequest = true;
                    
                    // 验证范围
                    if (startPos >= fileSize) {
                        sendError(resp, "Range Not Satisfiable", HttpResponse::k416RangeNotSatisfiable, conn);
                        return true;
                    }
                    
                    // 如果endPos未指定或超出文件大小，则使用文件大小-1
                    if (endPos >= fileSize) {
                        endPos = fileSize - 1;
                    }
                }
            }
            
            // 获取 HttpContext
            auto httpContext = std::static_pointer_cast<HttpContext>(conn->getContext());
            if (!httpContext) {
                LOG_ERROR << "HttpContext is null";
                sendError(resp, "Internal Server Error", HttpResponse::k500InternalServerError, conn);
                return true;
            }
            
            // 尝试获取已存在的下载上下文
            std::shared_ptr<FileDownContext> downContext = httpContext->getContext<FileDownContext>();
            
            if (!downContext) {
                // 创建新的下载上下文
                downContext = std::make_shared<FileDownContext>(filepath, originalFilename);
                httpContext->setContext(downContext);
                
                // 设置初始响应头
                if (isRangeRequest) {
                    resp->setStatusCode(HttpResponse::k206PartialContent);
                    resp->setStatusMessage("Partial Content");
                    resp->addHeader("Content-Range", 
                                  "bytes " + std::to_string(startPos) + "-" + 
                                  std::to_string(endPos) + "/" + 
                                  std::to_string(fileSize));
                } else {
                    resp->setStatusCode(HttpResponse::k200Ok);
                    resp->setStatusMessage("OK");
                }
                
                resp->setContentType("application/octet-stream");
                resp->addHeader("Content-Disposition", 
                              "attachment; filename=\"" + originalFilename + "\"");
                resp->addHeader("Accept-Ranges", "bytes");
                resp->addHeader("Connection", "keep-alive");
                
                // 设置文件指针到起始位置
                downContext->seekTo(startPos);
            }
            
            // 设置写完成回调，继续发送下一个数据块
            conn->setWriteCompleteCallback([this, downContext](const TcpConnectionPtr& connection) {
                // 读取下一个数据块
                std::string chunk;
                if (downContext->readNextChunk(chunk)) {
                    // 直接发送数据块
                    connection->send(chunk);
                    return true;
                } else {
                    // 下载完成，关闭连接
                    LOG_INFO << "Download complete, closing connection";
                    connection->shutdown();
                    return true;
                }
            });
            
            return true;
        }
        catch (const std::exception& e) {
            LOG_ERROR << "Error during file download: " << e.what();
            sendError(resp, "Download failed", HttpResponse::k500InternalServerError, conn);
            return true;
        }
    }

    // 获取分享信息
    bool handleShareInfo(const TcpConnectionPtr& conn, HttpRequest& req, HttpResponse* resp) {
        std::string shareCode = req.getPathParam("code");
        if (shareCode.empty()) {
            sendError(resp, "Missing share code", HttpResponse::k400BadRequest, conn);
            return true;
        }

        // 获取提取码（如果有）
        std::string extractCode = req.getQuery("extract_code", "");
        LOG_INFO << "shareCode = " << shareCode << ", extractCode = " << extractCode;

        // 查询分享信息
        std::string query = "SELECT fs.*, f.filename, f.original_filename, f.file_size, "
                          "f.file_type, u.username as owner_username, f.user_id "
                          "FROM file_shares fs "
                          "JOIN files f ON fs.file_id = f.id "
                          "JOIN users u ON f.user_id = u.id "
                          "WHERE fs.share_code = '" + escapeString(shareCode) + "' "
                          "AND (fs.expire_time IS NULL OR fs.expire_time > NOW())";
        
        LOG_INFO << "查询分享信息: " << query;
        MYSQL_RES* result = executeQueryWithResult(query);
        
        if (!result || mysql_num_rows(result) == 0) {
            if (result) mysql_free_result(result);
            LOG_ERROR << "分享链接已失效或不存在, shareCode = " << shareCode;
            sendError(resp, "分享链接已失效或不存在", HttpResponse::k404NotFound, conn);
            return true;
        }

        MYSQL_ROW row = mysql_fetch_row(result);
        std::string shareType = row[4];  // fs.share_type
        std::string dbExtractCode = row[8] ? row[8] : "";  // fs.extract_code
        std::string serverFilename = row[9] ? row[9] : "";
        std::string originalFilename = row[10] ? row[10] : "";
        uintmax_t fileSize = row[11] ? std::stoull(row[11]) : 0;
        std::string createdAt = row[7] ? row[7] : "";
        std::string expireTime = row[6] ? row[6] : "";
        
        // 如果是受保护的文件，需要验证提取码
        if (shareType == "protected") {
            if (extractCode.empty() || extractCode != dbExtractCode) {
                mysql_free_result(result);
                LOG_ERROR << "提取码错误或未提供, shareCode = " << shareCode;
                sendError(resp, "需要正确的提取码", HttpResponse::k403Forbidden, conn);
                return true;
            }
        }
        
        mysql_free_result(result);

        json response = {
            {"code", 0},
            {"message", "success"},
            {"shareType", shareType},
            {"file", {
                {"id", std::stoi(row[0])},
                {"name", serverFilename},
                {"originalName", originalFilename},
                {"size", fileSize},
                {"type", row[11] ? row[11] : "unknown"},
                {"shareTime", createdAt},
                {"expireTime", expireTime}
            }}
        };

        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("application/json");
        resp->addHeader("Connection", "close");
        resp->setBody(response.dump());

        conn->setWriteCompleteCallback([](const TcpConnectionPtr& connection) {
            connection->shutdown();
            return true;
        });
        
        return true;
    }

    // 处理 favicon.ico 请求
    bool handleFavicon(const TcpConnectionPtr& conn, HttpRequest& req, HttpResponse* resp) {
        // 获取当前文件目录
        std::string currentDir = __FILE__;
        std::string::size_type pos = currentDir.find_last_of("/");
        std::string projectRoot = currentDir.substr(0, pos);
        std::string faviconPath = projectRoot + "/favicon.ico";
        
        // 读取 favicon.ico 文件
        std::ifstream file(faviconPath, std::ios::binary);
        if (!file.is_open()) {
            LOG_ERROR << "Failed to open favicon.ico";
            resp->setStatusCode(HttpResponse::k404NotFound);
            resp->setStatusMessage("Not Found");
            resp->setContentType("image/x-icon");
            resp->addHeader("Connection", "close");
            resp->setBody("");
        } else {
            std::string iconData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
            
            resp->setStatusCode(HttpResponse::k200Ok);
            resp->setStatusMessage("OK");
            resp->setContentType("image/x-icon");
            resp->addHeader("Connection", "close");
            resp->setBody(iconData);
        }
        
        conn->setWriteCompleteCallback([](const TcpConnectionPtr& connection) {
            connection->shutdown();
            return true;
        });
        return true;
    }
};

int main() {
    Logger::setLogLevel(Logger::INFO);
    EventLoop loop;
    HttpServer server(&loop, InetAddress(8000), "http-upload-test");
    
    // 创建HTTP处理器
    auto handler = std::make_shared<HttpUploadHandler>(4);
    
    // 设置连接回调
    server.setConnectionCallback(
        [handler](const TcpConnectionPtr& conn) {
            handler->onConnection(conn);
        });
    
    // 设置HTTP回调
    server.setHttpCallback(
        [handler](const TcpConnectionPtr& conn, HttpRequest& req, HttpResponse* resp) {
            return handler->onRequest(conn, req, resp);
        });
    
    server.setThreadNum(0);
    server.start();
    std::cout << "HTTP upload server is running on port 8000..." << std::endl;
    std::cout << "Please visit http://localhost:8000" << std::endl;
    loop.loop();
    return 0;
}