#pragma once

#include <map>
#include <string>
#include <assert.h>
#include "base/copyable.h"
#include "base/Timestamp.h"
#include "base/Logging.h"
namespace mymuduo {
namespace net {
class HttpRequest : public mymuduo::copyable
{
 public:
  enum Method
  {
    kInvalid, kGet, kPost, kHead, kPut, kDelete
  };
  enum Version
  {
    kUnknown, kHttp10, kHttp11
  };

  HttpRequest()
    : method_(kInvalid),
      version_(kUnknown)
  {
  }

  void setVersion(Version v)
  {
    version_ = v;
  }

  Version getVersion() const
  { return version_; }

  bool setMethod(const char* start, const char* end)
  {
    assert(method_ == kInvalid);
    string m(start, end);
    if (m == "GET")
    {
      method_ = kGet;
    }
    else if (m == "POST")
    {
      method_ = kPost;
    }
    else if (m == "HEAD")
    {
      method_ = kHead;
    }
    else if (m == "PUT")
    {
      method_ = kPut;
    }
    else if (m == "DELETE")
    {
      method_ = kDelete;
    }
    else
    {
      method_ = kInvalid;
    }
    return method_ != kInvalid;
  }

  Method method() const
  { return method_; }

  const char* methodString() const
  {
    const char* result = "UNKNOWN";
    switch(method_)
    {
      case kGet:
        result = "GET";
        break;
      case kPost:
        result = "POST";
        break;
      case kHead:
        result = "HEAD";
        break;
      case kPut:
        result = "PUT";
        break;
      case kDelete:
        result = "DELETE";
        break;
      default:
        break;
    }
    return result;
  }

  void setPath(const char* start, const char* end)
  {
    path_.assign(start, end);
  }

  const string& path() const
  { return path_; }

  void setQuery(const char* start, const char* end)
  {
    query_.assign(start, end);
  }

  const string& query() const
  { return query_; }

  void setBody(const string& body) { body_ = body; }
  void appendToBody(const char* data, size_t len) { 
    LOG_DEBUG << "orig body size: " << body_.size() << ", append len: " << len;
    body_.append(data, len); 
    }
  const string& body() const { return body_; }

  void setReceiveTime(Timestamp t)
  { receiveTime_ = t; }

  Timestamp receiveTime() const
  { return receiveTime_; }

  void addHeader(const char* start, const char* colon, const char* end)
  {
    string field(start, colon);
    ++colon;
    while (colon < end && isspace(*colon))
    {
      ++colon;
    }
    string value(colon, end);
    while (!value.empty() && isspace(value[value.size()-1]))
    {
      value.resize(value.size()-1);
    }
    headers_[field] = value;
  }

  string getHeader(const string& field) const
  {
    string result;
    std::map<string, string>::const_iterator it = headers_.find(field);
    if (it != headers_.end())
    {
      result = it->second;
    }
    return result;
  }

  const std::map<string, string>& headers() const
  { return headers_; }

  void swap(HttpRequest& that)
  {
    std::swap(method_, that.method_);
    std::swap(version_, that.version_);
    path_.swap(that.path_);
    query_.swap(that.query_);
    body_.swap(that.body_);
    receiveTime_.swap(that.receiveTime_);
    headers_.swap(that.headers_);
  }

  // 获取查询参数
  std::string getQuery(const std::string& key, const std::string& defaultValue = "") const {
    std::string q = query();
    if (q.empty() || q[0] != '?') {
      return defaultValue;
    }
    q = q.substr(1);  // 移除开头的?
    
    std::vector<std::string> params;
    size_t start = 0;
    size_t end = q.find('&');
    while (end != std::string::npos) {
      params.push_back(q.substr(start, end - start));
      start = end + 1;
      end = q.find('&', start);
    }
    params.push_back(q.substr(start));
    
    for (const auto& param : params) {
      size_t pos = param.find('=');
      if (pos != std::string::npos) {
        std::string paramKey = param.substr(0, pos);
        if (paramKey == key) {
          return urlDecode(param.substr(pos + 1));
        }
      } else if (param == key) {
        return "true";
      }
    }
    
    return defaultValue;
  }
  
  // 获取路径参数
  std::string getPathParam(const std::string& name) const {
    auto it = pathParams_.find(name);
    if (it != pathParams_.end()) {
      return it->second;
    }
    return "";
  }
  
  // 设置路径参数
  void setPathParams(const std::unordered_map<std::string, std::string>& params) {
    pathParams_ = params;
  }
  
  // URL解码函数（简化版）
  std::string urlDecode(const std::string& encoded) const {
    std::string result;
    char ch;
    size_t i;
    size_t len = encoded.length();

    for (i = 0; i < len; i++) {
      if (encoded[i] != '%') {
        if (encoded[i] == '+')
          result += ' ';
        else
          result += encoded[i];
      } else {
        if (i + 2 < len) {
          int value;
          sscanf(encoded.substr(i + 1, 2).c_str(), "%x", &value);
          ch = static_cast<char>(value);
          result += ch;
          i = i + 2;
        }
      }
    }
    return result;
  }

 private:
  Method method_;
  Version version_;
  string path_;
  string query_;
  string body_;
  Timestamp receiveTime_;
  std::map<string, string> headers_;
  // 添加路径参数存储
  std::unordered_map<std::string, std::string> pathParams_;
};


} // namespace net
} // namespace mymuduo 