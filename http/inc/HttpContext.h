#pragma once

#include <string>
#include <memory>

#define CR '\r' // 回车
#define LF '\n' // 换行

enum HttpRequestParseState // 解析状态机
{
    INVALID,         // 无效
    INVALID_METHOD,  // 无效请求方法
    INVALID_URL,     // 无效请求路径
    INVALID_VERSION, // 无效的协议版本号
    INVALID_HEADER,  // 无效请求头

    START,  // 解析开始
    METHOD, // 请求方法

    BEFORE_URL, // 请求连接前的状态，需要'/'开头
    IN_URL,     // url处理

    BEFORE_URL_PARAM_KEY,   // URL请求参数键之前
    URL_PARAM_KEY,          // URL请求参数键
    BEFORE_URL_PARAM_VALUE, // URL请求参数值之前
    URL_PARAM_VALUE,        // URL请求参数值

    BEFORE_PROTOCOL, // 协议解析之前
    PROTOCOL,        // 协议

    BEFORE_VERSION, // 版本开始前
    VERSION_SPLIT,  // 版本分割符
    VERSION,        // 版本

    HEADER,
    HEADER_KEY, //

    HEADER_BEFORE_COLON, // 请求头冒号之前
    HEADER_AFTER_COLON,  // 请求头冒号
    HEADER_VALUE,        // 请求值

    WHEN_CR, // 遇到一个回车

    CR_LF, // 回车换行

    CR_LF_CR, // 回车换行之后的状态

    BODY, // 请求体

    COMPLETE, // 完成
};

class HttpRequest; // 前向声明

class HttpContext
{
private:
    std::shared_ptr<HttpRequest> request_; // 请求对象
    HttpRequestParseState state_;          // 当前解析状态

public:
    HttpContext();
    ~HttpContext();

    bool ParseRequest(const char *begin, int size); // 解析请求
    bool ParseRequest(const std::string &request); // 解析请求字符串
    bool ParseRequest(const char *begin);           // 解析请求起始位置
    bool GetCompleteRequest();                      // 获取完整请求
    HttpRequest *GetRequest();                      // 获取请求对象
    void ResetContextStatus();                      // 重置上下文状态
};