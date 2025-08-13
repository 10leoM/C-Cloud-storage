#include "HttpResponse.h"
#include "Buffer.h"

HttpResponse::HttpResponse(bool close_connection) : status_code_(HttpStatusCode::Unknown), close_connection_(close_connection), content_length_(0), filefd_(-1) {}

HttpResponse::~HttpResponse() {}

void HttpResponse::SetStatusCode(HttpStatusCode status_code) 
{
    status_code_ = status_code;
}

void HttpResponse::SetStatusMessage(const std::string &status_message)
{
    status_message_ = status_message;
}

void HttpResponse::SetCloseConnection(bool close_connection)
{
    close_connection_ = close_connection;
}

void HttpResponse::SetBody(const std::string &body)
{
    body_ = body;
}   

void HttpResponse::SetContentType(const std::string &content_type)
{
    AddHeader("Content-Type", content_type);
}

void HttpResponse::SetContentLength(const int &len)
{
    content_length_ = std::move(len);
}

void HttpResponse::AddHeader(const std::string &key, const std::string &value)
{
    headers_[key] = value;
}

bool HttpResponse::IsCloseConnection()
{
    return close_connection_;
}   

int HttpResponse::GetContentLength()
{
    return content_length_;
}

int HttpResponse::GetFileFd()
{
    return filefd_;
}

HttpBodyType HttpResponse::GetBodyType()
{
    return body_type_;
}

void HttpResponse::SetFileFd(int filefd)
{
    filefd_ = std::move(filefd);
}

void HttpResponse::SetBodyType(HttpBodyType bodytype)
{
    body_type_ = std::move(bodytype);
}

std::string HttpResponse::GetBeforeBody()
{
    std::string message;
    message += "HTTP/1.1 " + std::to_string(status_code_) + " " + status_message_ + "\r\n";

    if (close_connection_)
    {
        message += "Connection: close\r\n";
    }
    else
    {
        message += "Content-Length: " + std::to_string(content_length_) + "\r\n";
        message += "Connection: Keep-Alive\r\n";
    }

    // Range 处理：若指定区间覆盖则输出 Content-Range 并调整 Content-Length/状态码
    if (has_range_) {
        if (status_code_ == HttpStatusCode::OK) status_code_ = static_cast<HttpStatusCode>(206); // Partial Content
        std::string cr = "bytes " + std::to_string(range_start_) + "-" + std::to_string(range_end_);
        if (total_length_ >= 0) cr += "/" + std::to_string(total_length_); else cr += "/*";
        headers_["Content-Range"] = cr;
        content_length_ = static_cast<int>(range_end_ - range_start_ + 1);
    }
    for(auto &header : headers_) message += header.first + ": " + header.second + "\r\n";
    message += "\r\n";

    return message;
}

std::string HttpResponse::GetMessage()
{
    return GetBeforeBody() + body_;   
}

void HttpResponse::AppendToBuffer(Buffer* out) const
{
    if (!out) return;
    std::string head;
    head.reserve(256 + headers_.size()*32);
    head += "HTTP/1.1 " + std::to_string(status_code_) + " " + status_message_ + "\r\n";
    if (close_connection_) head += "Connection: close\r\n"; else head += "Connection: Keep-Alive\r\n";
    // 若未显式 Content-Length 且非关闭连接，则计算
    if (!has_range_) {
        if (!close_connection_ && content_length_ == 0 && !body_.empty())
            head += "Content-Length: " + std::to_string(body_.size()) + "\r\n";
        else if (content_length_ > 0)
            head += "Content-Length: " + std::to_string(content_length_) + "\r\n";
    } else {
        std::string cr = "bytes " + std::to_string(range_start_) + "-" + std::to_string(range_end_);
        if (total_length_ >= 0) cr += "/" + std::to_string(total_length_); else cr += "/*";
        head += "Content-Range: " + cr + "\r\n";
        head += "Content-Length: " + std::to_string(range_end_ - range_start_ + 1) + "\r\n";
        if (status_code_ == HttpStatusCode::OK) ; // 调用方应设为206，保守不改写
    }
    for (auto &kv : headers_) {
        if (kv.first == "Content-Length" || kv.first == "Content-Range" || kv.first == "Connection") continue; // 已输出或覆盖
        head += kv.first + ": " + kv.second + "\r\n";
    }
    head += "\r\n";
    out->Append(head.data(), head.size());
    if (!body_.empty()) out->Append(body_.data(), body_.size());
}

bool HttpResponse::SetContentRange(long long start, long long end, long long total)
{
    if (start < 0 || end < start) return false;
    range_start_ = start; range_end_ = end; total_length_ = total; has_range_ = true; return true;
}

