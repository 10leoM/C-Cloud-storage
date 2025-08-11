#include <iostream>
#include "HttpServer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "EventLoop.h"
#include "Logger.h"
#include "AsyncLogging.h"
#include <string>
#include <memory>
#include <fstream>
#include <vector>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

std::string ReadFile(const std::string &path)
{
    std::ifstream is(path.c_str(), std::ifstream::in); // 读方式打开文件
    if (!is.is_open())
    {
        std::cout << "无法打开文件: " << path << std::endl;
        return "open error\n";
    }

    is.seekg(0, is.end); // 定位到文件末尾

    int flength = is.tellg(); // 获取文件长度

    is.seekg(0, is.beg); // 定位到文件开头

    char *buffer = new char[flength];

    is.read(buffer, flength); // 读取文件

    std::string msg(buffer, flength); // 转换为字符串

    delete[] buffer; // 释放内存

    return msg; // 返回字符串
}

// 查找当前目录下所有文件的名字
void FindAllFiles(const std::string &path, std::vector<std::string> &filelist)
{
    DIR *dir;
    struct dirent *dir_entry = nullptr;

    if ((dir = opendir(path.c_str())) == nullptr)
    {
        LOG_ERROR << "Opendir " << path << " failed";
        return;
    }

    while ((dir_entry = readdir(dir)) != nullptr)
    {
        std::string filename = dir_entry->d_name;
        if (filename != "." && filename != "..") // 排除当前目录和上级目录
        {
            filelist.push_back(filename); // 添加到文件列表
        }
        else
        {
            LOG_ERROR << "Readir " << path << " failed";
        }
    }
    closedir(dir); // 关闭目录
}

//

// 构建文件列表的HTML
std::string BuildFileHtml()
{
    std::vector<std::string> filelist;
    FindAllFiles("../../files", filelist); // 查找../files目录下的所有文件
    std::string file = "";                 // 用于存储文件列表的HTML内容
    for (const auto &filename : filelist)
    {
        // 为每个文件生成HTML行
        file += "<tr><td>" + filename + "</td>" +
                "<td>" +
                "<a href=\"/download/" + filename + "\">下载</a>" +
                "<a href=\"/delete/" + filename + "\">删除</a>" +
                "</td></tr>\n";
    }

    // 生成html页面
    //  主要通过将<!--filelist-->直接进行替换实现
    std::string tmp = "<!--filelist-->";
    std::string filehtml = ReadFile("../static/fileserver.html");
    filehtml = filehtml.replace(filehtml.find(tmp), tmp.size(), file);
    return filehtml;
}

void RemoveFile(const std::string &filename)
{
    int ret = remove(("../files/" + filename).c_str());
    if (ret != 0)
    {
        LOG_ERROR << "删除文件 " << filename << " 失败";
    }
    else
    {
        LOG_INFO << "删除文件 " << filename << " 成功";
    }
}

void DownloadFile(const std::string &filename, HttpResponse *response)
{
    int filefd = open(("../../files/" + filename).c_str(), O_RDONLY);
    if (filefd < 0)
    {
        LOG_ERROR << "打开文件 " << filename << " 失败";
        response->SetStatusCode(HttpStatusCode::k302K);
        response->SetStatusMessage("Moved Temporarily");
        response->SetContentType("text/html");
        response->AddHeader("Location", "/fileserver");
    }
    else
    {
        struct stat file_stat;
        if (fstat(filefd, &file_stat) < 0)
        {
            LOG_ERROR << "获取文件状态失败";
            close(filefd);
            return;
        }
        response->SetStatusCode(HttpStatusCode::OK);
        response->SetContentLength(file_stat.st_size);
        response->SetContentType("application/octet-stream");
        response->SetBodyType(HttpBodyType::FILE_TYPE);
        response->SetFileFd(filefd);
    }
}

bool HttpResponseCallback(const std::shared_ptr<Connection> &/*conn*/, const HttpRequest &request, HttpResponse *response)
{
    LOG_INFO << request.GetMethodString() << " " << request.GetUrl();
    std::string url = request.GetUrl();
    if (request.GetMethod() == HttpMethod::kGet)
    {
        if (url == "/")
        {
            std::string body = ReadFile("../../static/index.html");
            response->SetStatusCode(HttpStatusCode::OK);
            response->SetContentLength(body.size());
            response->SetBody(body);
            response->SetContentType("text/html");
        }
        else if (url == "/mhw")
        {
            std::string body = ReadFile("../../static/mhw.html");
            response->SetStatusCode(HttpStatusCode::OK);
            response->SetContentLength(body.size());
            response->SetBody(body);
            response->SetContentType("text/html");
        }
        else if (url == "/cat.jpg")
        {
            std::string body = ReadFile("../../static/cat.jpg");
            response->SetStatusCode(HttpStatusCode::OK);
            response->SetContentLength(body.size());
            response->SetBody(body);
            response->SetContentType("image/jpeg");
        }
        else if (url == "/fileserver")
        {
            std::string body = BuildFileHtml();
            response->SetStatusCode(HttpStatusCode::OK);
            response->SetContentLength(body.size());
            response->SetBody(body);
            response->SetContentType("text/html");
        }
        else if (url.substr(0, 7) == "/delete")
        {
            // 删除特定文件，由于使用get请求，并且会将相应删掉文件的名称放在url中
            RemoveFile(url.substr(8));
            // 发送重定向报文，删除后返回自身应在的位置
            response->SetStatusCode(HttpStatusCode::OK);
            response->SetStatusMessage("Moved Temporarily");
            response->SetContentType("text/html");
            response->AddHeader("Location", "/fileserver");
        }
        else if (url.substr(0, 9) == "/download")
        {
            DownloadFile(url.substr(10), response);
            // response->SetStatusCode(HttpResponse::HttpStatusCode::k200K);
        }
        else if (url == "/favicon.ico")
        {
            std::string body = ReadFile("../../static/cat.jpg");
            response->SetStatusCode(HttpStatusCode::OK);
            response->SetContentLength(body.size());
            response->SetBody(body);
            response->SetContentType("image/jpeg");
        }
        else
        {
            response->SetStatusCode(HttpStatusCode::NotFound);
            response->SetStatusMessage("Not Found");
            response->SetBody("Sorry Not Found\n");
            response->SetCloseConnection(true);
        }
    }
    else if (request.GetMethod() == HttpMethod::kPost)
    {
        if (url == "/login")
        {
            // 进入登陆界面
            std::string rqbody = request.GetBody();

            // 解析
            int usernamePos = rqbody.find("username=");
            int passwordPos = rqbody.find("password=");
            usernamePos += 9; // "username="的长度
            passwordPos += 9; // "password="的长度

            // 找到中间分割符
            size_t usernameEndPos = rqbody.find('&', usernamePos);
            size_t passwordEndPos = rqbody.length();

            // 提取用户名和密码子串
            std::string username = rqbody.substr(usernamePos, usernameEndPos - usernamePos);
            std::string password = rqbody.substr(passwordPos, passwordEndPos - passwordPos);

            // 打印用户名和密码
            std::cout << "Username: " << username << std::endl;
            std::cout << "Password: " << password << std::endl;

            // 登陆验证
            if (username == "admin" && password == "123456")
            {
                response->SetStatusCode(HttpStatusCode::OK);
                response->SetBody("login success");
                response->SetContentType("text/plain");
            }
            else
            {
                response->SetStatusCode(HttpStatusCode::OK);
                response->SetBody("login fail");
                response->SetContentType("text/plain");
            }
        }
        else if(url == "/upload")
        {
            response->SetStatusCode(HttpStatusCode::k302K);
            response->SetStatusMessage("Moved Temporarily");
            response->SetContentType("text/html");
        response->AddHeader("Location", "/fileserver");
        }
    }
    return true; // 同步发送
}

std::unique_ptr<AsyncLogging> asynclog;
void AsyncOutputFunc(const char *data, int len)
{
    asynclog->Append(data, len);
}

void AsyncFlushFunc()
{
    asynclog->Flush();
}

int main(int argc, char *argv[])
{
    int port;
    if (argc <= 1)
    {
        port = 8080;
    }
    else if (argc == 2)
    {
        port = atoi(argv[1]);
    }
    else
    {
        printf("error");
        exit(0);
    }

    // asynclog = std::make_unique<AsyncLogging>();
    // Logger::SetOutput(AsyncOutputFunc);
    // Logger::SetFlush(AsyncFlushFunc);

    // asynclog->Start();

    int size = std::thread::hardware_concurrency() - 1;
    EventLoop *loop = new EventLoop();
    HttpServer *server = new HttpServer(loop, "127.0.0.1", port, true);
    server->SetHttpCallback(HttpResponseCallback);
    server->SetThreadNums(size);
    server->start();

    // delete loop;
    // delete server;
    return 0;
}