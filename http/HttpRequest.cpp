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
    // 拆分 ? 之前为 path, 之后为 query
    auto pos = url.find('?');
    if (pos == std::string::npos) {
        url_ = url;
        raw_query_.clear();
    } else {
        url_ = url.substr(0, pos);
        raw_query_ = url.substr(pos + 1);
    }
    // 清空旧查询参数并重新解析
    query_params_multi_.clear();
    if (!raw_query_.empty()) {
        ParseQueryString();
    }
}

const std::string &HttpRequest::GetUrl() const 
{
    return url_;
}

const std::string &HttpRequest::GetRawQuery() const { return raw_query_; }

std::string HttpRequest::GetQueryValue(const std::string &key) const {
    auto it = query_params_multi_.find(key);
    if (it != query_params_multi_.end() && !it->second.empty()) return it->second.front();
    return {};
}
const std::vector<std::string> &HttpRequest::GetQueryValues(const std::string &key) const {
    static const std::vector<std::string> kEmpty;
    auto it = query_params_multi_.find(key);
    if (it != query_params_multi_.end()) return it->second;
    return kEmpty;
}
const std::map<std::string, std::vector<std::string>> &HttpRequest::GetQueryParamMap() const { return query_params_multi_; }

void HttpRequest::SetPathParam(const std::string& key, const std::string& value) { path_params_[key] = value; }
std::string HttpRequest::GetPathParam(const std::string& key) const {
    auto it = path_params_.find(key);
    if (it != path_params_.end()) return it->second;
    return {};
}
const std::map<std::string,std::string>& HttpRequest::GetPathParams() const { return path_params_; }

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

void HttpRequest::AppendBody(const char* data, size_t len)
{
    if (data && len) body_.append(data, len);
}

// URL Decode 实现（处理 %XX 与 + -> space）
std::string HttpRequest::UrlDecode(const std::string &src)
{
    std::string out;
    out.reserve(src.size());
    for (size_t i = 0; i < src.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(src[i]);
        if (c == '+') { out.push_back(' '); }
        else if (c == '%' && i + 2 < src.size()) {
            auto hex = [](unsigned char h)->int {
                if (h >= '0' && h <= '9') return h - '0';
                if (h >= 'a' && h <= 'f') return h - 'a' + 10;
                if (h >= 'A' && h <= 'F') return h - 'A' + 10;
                return -1;
            };
            int h1 = hex(static_cast<unsigned char>(src[i+1]));
            int h2 = hex(static_cast<unsigned char>(src[i+2]));
            if (h1 >= 0 && h2 >= 0) {
                out.push_back(static_cast<char>((h1 << 4) | h2));
                i += 2;
            } else {
                out.push_back(c); // 非法编码，按原样输出
            }
        } else {
            out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

void HttpRequest::AddQueryParam(const std::string &key, const std::string &value)
{
    query_params_multi_[key].push_back(value);
}

void HttpRequest::ParseQueryString()
{
    size_t start = 0;
    while (start < raw_query_.size()) {
        size_t amp = raw_query_.find('&', start);
        if (amp == std::string::npos) amp = raw_query_.size();
        std::string pair = raw_query_.substr(start, amp - start);
        if (!pair.empty()) {
            size_t eq = pair.find('=');
            std::string key, value;
            if (eq == std::string::npos) {
                key = UrlDecode(pair);
            } else {
                key = UrlDecode(pair.substr(0, eq));
                value = UrlDecode(pair.substr(eq + 1));
            }
            if (!key.empty()) AddQueryParam(key, value);
        }
        start = amp + 1;
    }
}

bool HttpRequest::ExtractPathParams(const std::string &pattern)
{
    // 简单分段匹配: /a/:id/b ；:name 为变量
    // 不支持通配符 * / 正则。
    auto split = [](const std::string &s){
        std::vector<std::string> v; size_t i=0; while(i<s.size()){ while(i<s.size() && s[i]=='/') ++i; if(i>=s.size()) break; size_t j=i; while(j<s.size() && s[j]!='/') ++j; v.emplace_back(s.substr(i,j-i)); i=j; } return v; };
    auto pathSegs = split(url_);
    auto patSegs = split(pattern);
    if (pathSegs.size() != patSegs.size()) return false;
    for (size_t i=0;i<patSegs.size();++i){
        const std::string &p = patSegs[i];
        const std::string &seg = pathSegs[i];
        if (!p.empty() && p[0]==':') {
            std::string key = p.substr(1);
            path_params_[key] = seg; // 覆盖旧值
        } else if (p != seg) {
            return false;
        }
    }
    return true;
}

// Range: bytes=start-end 或 bytes=-suffix
bool HttpRequest::()
{
    has_range_ = false; range_start_ = 0; range_end_ = -1; range_suffix_ = false;
    auto it = headers_.find("Range");
    if (it == headers_.end()) return false;
    const std::string& val = it->second;
    if (val.size() < 6 || val.substr(0,6) != "bytes=") return false;
    std::string spec = val.substr(6);
    auto dash = spec.find('-');
    if (dash == std::string::npos) return false;
    std::string start = spec.substr(0, dash);
    std::string end = spec.substr(dash+1);
    if (start.empty() && !end.empty()) {
        // bytes=-N
        range_suffix_ = true;
        try { range_end_ = std::stoll(end); } catch (...) { return false; }
        has_range_ = true;
        return true;
    }
    try { range_start_ = std::stoll(start); } catch (...) { return false; }
    if (!end.empty()) {
        try { range_end_ = std::stoll(end); } catch (...) { return false; }
    } else {
        range_end_ = -1;
    }
    has_range_ = true;
    return true;
}

// 其他成员函数的实现