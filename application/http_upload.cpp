#include "HttpServer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "EventLoop.h"
#include "HttpContext.h"
#include "Logger.h"
#include "FileUploadContext.h"
#include "FileDownContext.h"
#include "src/inc/Db.h"
#include "src/inc/Util.h"
#include "src/inc/FilenameMap.h"
#include "src/inc/AuthHandler.h"
#include "src/inc/StaticHandler.h"
#include "src/inc/FileHandler.h"
#include "src/inc/ShareHandler.h"
#include "src/inc/UserHandler.h"
#include "src/inc/Router.h"
#include "src/inc/Db.h"
#include "src/inc/HttpUtil.h"
#include "Connection.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <map>
#include <string>
#include <memory>
#include <mutex>
#include <functional>
#include <fstream>
#include <experimental/filesystem>
#include <vector>
#include <cstdio>
#include <unistd.h>
#include <mysql/mysql.h>

using json = nlohmann::json;
namespace fs = std::experimental::filesystem;

class HttpUploadHandler
{
private:
    std::string uploadDir_;   // 上传目录
    std::string mappingFile_; // 文件名映射文件
    FilenameMap filenameMap_; // 文件名映射
    // 数据库封装
    Db db_;
    AuthHandler auth_;     // 认证与会话
    StaticHandler static_; // 静态资源
    FileHandler file_;     // 文件相关处理（list/delete/...）
    ShareHandler share_;   // 分享相关处理
    UserHandler user_;     // 用户相关处理（搜索等）

    Router router_;

    // 初始化数据库连接
    bool initDatabase() { return db_.connect(); }

    // 关闭数据库连接
    void closeDatabase() { /* Db 析构自动关闭 */ }

    // 直接使用各 Handler/Repository 进行数据访问

public:
    HttpUploadHandler(int numThreads,
                      const std::string &dbHost = "localhost",
                      const std::string &dbUser = "root",
                      const std::string &dbPassword = "123456",
                      const std::string &dbName = "file_manager",
                      unsigned int dbPort = 3306)
        : uploadDir_("uploads"), mappingFile_("uploads/filename_mapping.json"), 
        filenameMap_(mappingFile_), db_(dbHost, dbUser, dbPassword, dbName, dbPort), 
        auth_(db_), file_(db_, auth_, filenameMap_, uploadDir_), 
        share_(db_, auth_, static_, uploadDir_), user_(db_, auth_)
    {
        (void)numThreads; // 线程池已移除，参数保留以兼容构造调用

        // 创建上传目录
        if (!fs::exists(uploadDir_))
        {
            fs::create_directory(uploadDir_);
        }

        // 初始化数据库连接
        if (!initDatabase())
        {
            LOG_ERROR << "数据库初始化失败，系统可能无法正常工作";
        }

        // 加载文件名映射
        filenameMap_.load();

        // 初始化路由表
        initRoutes();
    }

    ~HttpUploadHandler()
    {
        // 保存文件名映射
        filenameMap_.save();
        closeDatabase();
    }

    void onConnection(const std::shared_ptr<Connection> &conn)
    {
        if (conn->GetState() == connectionState::Connected)
        {
            LOG_INFO << "New connection from " << conn->GetpeerIpPort();
            // 为每个新连接创建一个 HttpContext
            conn->SetContext(std::make_shared<HttpContext>());
        }
        else
        {
            LOG_INFO << "Connection closed from " << conn->GetpeerIpPort();
            // 清理上下文
            if (auto context = conn->GetContext()->GetContext<FileUploadContext>())
            {
                    LOG_INFO << "Cleaning up upload context for file: " << context->getFilename();
            }
            conn->SetContext(std::shared_ptr<HttpContext>());
        }
    }

    // 返回 true 表示同步处理完成，false 表示异步处理
    bool HttpCallback(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp)
    {
        std::string path = req.GetUrl();
        LOG_INFO << "Headers " << req.GetMethodString() << " " << path;
        LOG_INFO << "Content-Type: " << req.GetHeader("Content-Type");
        LOG_INFO << "Body size: " << req.GetBody().size();

        try
        {
            // 交给路由分发
            if (router_.dispatch(conn, req, resp))
                return true;

            // 未找到匹配的路由，返回404
            LOG_WARN << "No matching route found for " << path;
            // 统一404
            sendError(resp, "Not Found", HttpStatusCode::NotFound, conn);
            return true;
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Error processing request: " << e.what();
            sendError(resp, "Internal Server Error", HttpStatusCode::InternalServerError, conn);
            return true;
        }
    }

private:
    // 旧版上传逻辑已移除，现统一走 FileHandler::handleUpload

    bool handleListFiles(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp)
    {
        return file_.handleListFiles(conn, req, resp);
    }

    bool handleDownload(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp) { return file_.handleDownload(conn, req, resp); }

    bool handleDelete(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp)
    {
        return file_.handleDelete(conn, req, resp);
    }

    // 统一错误处理已迁移到 HttpUtil

    // 文件名映射 API（委托给 FilenameMap）
    void addFilenameMapping(const std::string &serverFilename, const std::string &originalFilename) { filenameMap_.add(serverFilename, originalFilename); }
    std::string getOriginalFilename(const std::string &serverFilename) { return filenameMap_.getOriginal(serverFilename); }
    std::map<std::string, std::string> getAllFileMappings() { return filenameMap_.getAll(); }

    // 用户注册
    bool handleRegister(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp)
    {
        return auth_.handleRegister(conn, req, resp);
    }

    // 用户登录
    bool handleLogin(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp)
    {
        return auth_.handleLogin(conn, req, resp);
    }

    // 会话管理已在各 Handler 内部处理
private:
    void initRoutes()
    {
        registerRoutes(router_, static_, auth_, file_, share_, user_);
    }

    // 路由注册已迁移至 Router

    // 简单转发包装已移除，直接在 Routes 中绑定到各 Handler
};

int main()
{
    Logger::SetLogLevel(Logger::INFO);
    EventLoop loop;
    // 监听 0.0.0.0 以便通过 localhost(127.0.0.1) 或本机 IP 访问
    HttpServer server(&loop, "0.0.0.0", 8080, false);

    // 创建HTTP处理器
    auto handler = std::make_shared<HttpUploadHandler>(4);

    // 设置连接回调
    server.SetOnConnectionCallback(
        [handler](const std::shared_ptr<Connection> &conn)
        {
            handler->onConnection(conn);
        });

    // 设置HTTP回调
    server.SetHttpCallback(
        [handler](const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp)
        {
            return handler->HttpCallback(conn, req, resp);
        });

    server.SetThreadNums(std::thread::hardware_concurrency());
    std::cout << "HTTP upload server is running on port 8080..." << std::endl;
    std::cout << "Please visit http://localhost:8080" << std::endl;
    server.start();
    // 进入事件循环，保持服务运行
    loop.loop();
    return 0;
}