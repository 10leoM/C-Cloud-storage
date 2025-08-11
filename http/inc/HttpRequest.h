#pragma once

#include <string>
#include <memory>
#include <map>
#include <vector>

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

    std::string url_;                                                    // 原始路径(不含query)
    std::string raw_query_;                                              // 原始查询串  key1=val1&key2=val2
    std::map<std::string, std::vector<std::string>> query_params_multi_; // 支持重复 key: k=a&k=b  -> query_params_multi_["k"] = {"a","b"}
    std::map<std::string, std::string> path_params_;                     // 路由解析出的动态路径参数 /user/:id
    std::map<std::string, std::string> request_params_;                  // 请求参数
    std::string protocol_;                                               // 协议
    std::map<std::string, std::string> headers_;                         // 请求头
    std::string body_;                                                   // 请求体

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
    // 设置原始URL(可能包含 ?query)；内部拆分 path 与 query
    void SetUrl(const std::string &url);
    const std::string &GetUrl() const;
    const std::string &GetRawQuery() const; // 原始 query 子串
    std::string GetQueryValue(const std::string &key) const;
    // Query 访问：GetQueryValue 返回首个值；GetQueryValues 返回全部；GetQueryParamMap 返回映射
    const std::vector<std::string> &GetQueryValues(const std::string &key) const;
    const std::map<std::string, std::vector<std::string>> &GetQueryParamMap() const;

    // 手动注入路径参数(路由匹配阶段调用)
    void SetPathParam(const std::string &key, const std::string &value);
    std::string GetPathParam(const std::string &key) const;
    const std::map<std::string, std::string> &GetPathParams() const;

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

    // 工具
    static std::string UrlDecode(const std::string &src);
    void ParseQueryString();                                              // 在 SetUrl 内部调用或外部重新触发
    void AddQueryParam(const std::string &key, const std::string &value); // 多值追加

    // 路径参数自动提取：pattern 形如 /user/:id/books/:bid ，成功则写入 path_params_
    bool ExtractPathParams(const std::string &pattern);
};