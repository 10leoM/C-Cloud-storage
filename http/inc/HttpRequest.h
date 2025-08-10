#pragma once

#include <string>
#include <memory>
#include <map>

enum HttpMethod
{
    kInvalid = 0,
    kGet,
    kPost,
    kHead,
    kPut,
    kDelete
};

enum HttpVersion
{
    kUnknown = 0,
    kHttp10,
    kHttp11,
};

class HttpRequest
{
private:
    HttpMethod method_;   // 请求方法
    HttpVersion version_; // 版本

    std::string url_;                                   // 请求路径
    std::map<std::string, std::string> request_params_; // 请求参数
    std::string protocol_;                              // 协议
    std::map<std::string, std::string> headers_;        // 请求头
    std::string body_;                                  // 请求体

public:
    HttpRequest();
    ~HttpRequest();

    // 设定请求方法
    void SetMethod(const std::string &method);
    HttpMethod GetMethod() const;
    std::string GetMethodString() const;

    // http版本
    void SetVersion(const std::string &ver);
    HttpVersion GetVersion() const;
    std::string GetVersionString() const;

    // 请求路径
    void SetUrl(const std::string &url);
    const std::string &GetUrl() const;

    // 请求参数
    void SetRequestParams(const std::string &key, const std::string &value);
    std::string GetRequestValue(const std::string &key) const;
    const std::map<std::string, std::string> &GetRequestParams() const;

    // 协议
    void SetProtocol(const std::string &str);
    const std::string &GetProtocol() const;

    // 添加请求头
    void AddHeader(const std::string &field, const std::string &value);
    std::string GetHeader(const std::string &field) const;
    const std::map<std::string, std::string> &GetHeaders() const;
    
    // 请求体
    void SetBody(const std::string &str);
    const std::string &GetBody() const;
};