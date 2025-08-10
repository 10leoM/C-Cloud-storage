#include "HttpRequest.h"
#include <memory>
#include <string>
#include <algorithm>

HttpRequest::HttpRequest():method_(HttpMethod::kInvalid), version_(HttpVersion::kUnknown) {}

HttpRequest::~HttpRequest() {}

void HttpRequest::SetMethod(const std::string &method) 
{
    method_ = HttpMethod::kInvalid; // 默认无效
    if (method == "GET") 
        method_ = HttpMethod::kGet;
    else if (method == "POST")
        method_ = HttpMethod::kPost;
    else if (method == "HEAD")
        method_ = HttpMethod::kHead;
    else if (method == "PUT")
        method_ = HttpMethod::kPut;
    else if (method == "DELETE")
        method_ = HttpMethod::kDelete;
}

HttpMethod HttpRequest::GetMethod() const 
{
    return method_;
}

std::string HttpRequest::GetMethodString() const
{
    std::string method;

    if (method_ == HttpMethod::kGet)
        method = "GET";
    else if (method_ == HttpMethod::kPost)
        method = "POST";    
    else if (method_ == HttpMethod::kHead)
        method = "HEAD";
    else if (method_ == HttpMethod::kPut)
        method = "PUT";
    else if (method_ == HttpMethod::kDelete)
        method = "DELETE"; 
    
    return method.empty() ? "INVALID" : method;
}

void HttpRequest::SetVersion(const std::string &ver)
{
    version_ = HttpVersion::kUnknown; // 默认未知
    if (ver == "1.0")
        version_ = HttpVersion::kHttp10;
    else if (ver == "1.1")
        version_ = HttpVersion::kHttp11;
}

HttpVersion HttpRequest::GetVersion() const 
{
    return version_;
}

std::string HttpRequest::GetVersionString() const 
{
    std::string ver;

    if (version_ == HttpVersion::kHttp10)
        ver = "HTTP/1.0";
    else if (version_ == HttpVersion::kHttp11)
        ver = "HTTP/1.1";
    else
        ver = "Unknown";

    return ver;
}

void HttpRequest::SetUrl(const std::string &url) 
{
    url_ = url; // 直接赋值
}

const std::string &HttpRequest::GetUrl() const 
{
    return url_;
}

void HttpRequest::SetRequestParams(const std::string &key, const std::string &value) 
{
    request_params_[key] = value; // 使用map存储参数
}

std::string HttpRequest::GetRequestValue(const std::string &key) const 
{
    auto it = request_params_.find(key);
    if (it != request_params_.end())
        return it->second;
    return {};
}

const std::map<std::string, std::string> &HttpRequest::GetRequestParams() const 
{
    return request_params_;
}

void HttpRequest::SetProtocol(const std::string &str) 
{
    protocol_ = str; // 设置协议字符串
}

const std::string &HttpRequest::GetProtocol() const 
{
    return protocol_;
}

void HttpRequest::AddHeader(const std::string &field, const std::string &value) 
{
    headers_[field] = value; // 添加请求头
}

std::string HttpRequest::GetHeader(const std::string &field) const 
{
    auto it = headers_.find(field); 
    if (it != headers_.end())
        return it->second;
    return {};
}

const std::map<std::string, std::string> &HttpRequest::GetHeaders() const 
{
    return headers_;
}

void HttpRequest::SetBody(const std::string &str) 
{
    body_ = str; // 设置请求体
}   

const std::string &HttpRequest::GetBody() const
{
    return body_;
}

// 其他成员函数的实现