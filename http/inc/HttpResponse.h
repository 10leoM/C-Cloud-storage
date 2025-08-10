#pragma once

#include <string>
#include <utility>
#include <map>

enum HttpStatusCode
{
    Unknown = 0,              // 未知状态码
    Continue = 100,           // 继续
    OK = 200,                 // 成功
    k301K = 301,              // 永久重定向
    k302K = 302,              // 临时重定向
    BadRequest = 400,         // 错误请求
    Forbidden = 403,          // 禁止访问
    NotFound = 404,           // 未找到
    InternalServerError = 500 // 服务器内部错误
};

enum HttpBodyType
{
    HTML_TYPE,
    FILE_TYPE,
};

class HttpResponse
{
private:
    HttpStatusCode status_code_; // HTTP状态码
    std::string status_message_; // 状态码对应的消息
    std::string body_;           // 响应体内容
    bool close_connection_;      // 是否关闭连接

    std::map<std::string, std::string> headers_; // 响应头
    int content_length_;                         // 内容长度
    int filefd_;                                 // 文件描述符，用于文件传输
    HttpBodyType body_type_;                     // 响应体类型

public:
    HttpResponse(bool close_connection);
    ~HttpResponse();

    void SetStatusCode(HttpStatusCode status_code);                   // 设置状态码
    void SetStatusMessage(const std::string &status_message);         // 设置状态消息
    void SetCloseConnection(bool close_connection);                   // 设置连接关闭标志
    void SetBody(const std::string &body);                            // 设置响应体内容
    void SetContentType(const std::string &content_type);             // 设置内容类型
    void AddHeader(const std::string &key, const std::string &value); // 添加响应头
    void SetContentLength(const int &len);                        // 设置内容长度
    void SetFileFd(int file_fd);                                      // 设置文件描述符
    void SetBodyType(HttpBodyType body_type);                         // 设置响应体类型
    HttpBodyType GetBodyType();                                       // 获取响应体类型
    int GetContentLength();                                           // 获取内容长度
    int GetFileFd();                                                  // 获取文件描述符

    bool IsCloseConnection(); // 检查是否关闭连接

    std::string GetMessage();    // 生成完整的HTTP响应消息
    std::string GetBeforeBody(); // 先发送beforebody;
};