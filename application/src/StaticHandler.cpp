#include "inc/StaticHandler.h"
#include <fstream>
#include "Connection.h"
#include "HttpResponse.h"
#include "HttpRequest.h"

bool StaticHandler::handleIndex(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp)
{
    resp->SetStatusCode(HttpStatusCode::OK);
    resp->SetStatusMessage("OK");
    resp->SetContentType("text/html; charset=utf-8");

    std::string path = req.GetUrl();
    std::string currentDir = __FILE__;                                                                 // 找到当前文件所在目录
    std::string::size_type pos = currentDir.substr(0, currentDir.find_last_of("/")).find_last_of("/"); // 获取上上级目录
    std::string projectRoot = currentDir.substr(0, pos);                                               // 获取项目根目录
    std::string staticDir = projectRoot + "/static";
    std::string filePath;
    if (path == "/")
        filePath = staticDir + "/index.html";
    else
        filePath = staticDir + path;

    std::ifstream file(filePath); // 打开静态文件
    if (!file.is_open())
    {
        resp->SetStatusCode(HttpStatusCode::NotFound);
        resp->SetStatusMessage("Not Found");
        resp->SetContentType("text/html; charset=utf-8");
        resp->SetBody("<h1>404 Not Found</h1>");
        if (conn)
            conn->setWriteCompleteCallback([](const std::shared_ptr<Connection> &c){ c->shutdown(); return true; });
        return true;
    }
    std::string body;
    std::string line;
    while (std::getline(file, line))
        body += line + "\n";
    file.close();
    resp->SetBody(body);
    if (conn)
        conn->setWriteCompleteCallback([](const std::shared_ptr<Connection> &c)
                                       { c->shutdown(); return true; });
    return true;
}

bool StaticHandler::handleFavicon(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp)
{
    std::string currentDir = __FILE__;                                                                 // 找到当前文件所在目录
    std::string::size_type pos = currentDir.substr(0, currentDir.find_last_of("/")).find_last_of("/"); // 获取上上级目录
    std::string projectRoot = currentDir.substr(0, pos);                                               // 获取项目根目录
    std::string staticDir = projectRoot + "/static";
    std::string filePath = staticDir + "/favicon.ico";
    std::ifstream file(filePath); // 打开静态文件
    if (!file.is_open())
    {
        resp->SetStatusCode(HttpStatusCode::NotFound);
        resp->SetStatusMessage("Not Found");
        resp->SetContentType("text/html; charset=utf-8");
        resp->SetBody("<h1>404 Not Found</h1>");
    }
    else
    {
        std::string iconData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()); // 读取图标数据
        resp->SetStatusCode(HttpStatusCode::OK);
        resp->SetStatusMessage("OK");
        resp->SetContentType("image/x-icon");
        resp->SetBody(iconData);
    }
    if (conn)
        conn->setWriteCompleteCallback([](const std::shared_ptr<Connection> &c){ c->shutdown(); return true; });
    return true;
}

bool StaticHandler::handleStaticAsset(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp)
{
    // 路径形如 /static/xxx
    std::string path = req.GetUrl();
    if (path.rfind("/static/", 0) != 0) {
        resp->SetStatusCode(HttpStatusCode::NotFound);
        resp->SetStatusMessage("Not Found");
        resp->AddHeader("Connection","close");
        resp->SetBody("");
        conn->setWriteCompleteCallback([](const std::shared_ptr<Connection>& c){ c->shutdown(); return true;});
        return true;
    }
    std::string currentDir = __FILE__;
    std::string::size_type pos = currentDir.find_last_of("/");
    std::string projectRoot = currentDir.substr(0, pos);
    std::string staticDir = projectRoot + "/static";
    std::string rel = path.substr(std::string("/static/").size());
    // 基本的路径穿越防护：禁止 ..
    if (rel.find("..") != std::string::npos) {
        resp->SetStatusCode(HttpStatusCode::Forbidden);
        resp->SetStatusMessage("Forbidden");
        resp->AddHeader("Connection","close");
        resp->SetBody("");
        conn->setWriteCompleteCallback([](const std::shared_ptr<Connection>& c){ c->shutdown(); return true;});
        return true;
    }
    std::string filePath = staticDir + "/" + rel;
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        resp->SetStatusCode(HttpStatusCode::NotFound);
        resp->SetStatusMessage("Not Found");
        resp->AddHeader("Connection","close");
        resp->SetBody("");
        conn->setWriteCompleteCallback([](const std::shared_ptr<Connection>& c){ c->shutdown(); return true;});
        return true;
    }
    // 简单的内容类型映射
    static const std::unordered_map<std::string, std::string> mime = {
        {".js", "application/javascript"},
        {".css", "text/css"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".svg", "image/svg+xml"},
        {".ico", "image/x-icon"},
        {".html", "text/html; charset=utf-8"},
        {".json", "application/json"},
        {".txt", "text/plain; charset=utf-8"},
        {".xml", "text/xml; charset=utf-8"}
    };
    std::string ext;
    auto dot = filePath.find_last_of('.');
    if (dot != std::string::npos) ext = filePath.substr(dot);
    auto it = mime.find(ext);
    std::string ct = it != mime.end() ? it->second : "application/octet-stream";
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    resp->SetStatusCode(HttpStatusCode::OK);
    resp->SetStatusMessage("OK");
    resp->SetContentType(ct);
    resp->AddHeader("Connection","close");
    resp->SetBody(data);
    conn->setWriteCompleteCallback([](const std::shared_ptr<Connection>& c){ c->shutdown(); return true;});
    return true;
}