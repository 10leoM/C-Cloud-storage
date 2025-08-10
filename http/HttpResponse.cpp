#include "HttpResponse.h"

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

    for(auto &header : headers_)
    {
        message += header.first + ": " + header.second + "\r\n";
    }
    message += "\r\n";

    return message;
}

std::string HttpResponse::GetMessage()
{
    return GetBeforeBody() + body_;   
}

