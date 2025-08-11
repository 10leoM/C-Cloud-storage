#include "HttpContext.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

HttpContext::HttpContext()
    : request_(std::make_shared<HttpRequest>()), state_(HttpRequestParseState::START) {}

HttpContext::~HttpContext() = default;

HttpRequest *HttpContext::GetRequest()
{
    return request_.get();
}

bool HttpContext::GetCompleteRequest()
{
    return state_ == HttpRequestParseState::COMPLETE;
}

void HttpContext::ResetContextStatus()
{
    request_.reset(new HttpRequest());
    state_ = HttpRequestParseState::START;
    deferred_response_.reset();
    headers_complete_ = false;
    body_complete_ = false;
    chunked_ = false;
    content_length_ = 0;
    received_body_bytes_ = 0;
    chunk_state_ = ChunkState::SIZE;
    current_chunk_size_ = 0;
    chunk_size_buf_.clear();
}
bool HttpContext::ParseRequest(const char *begin, int size)
{
    // 为向后兼容，仍旧一次性解析（首阶段：请求行与头部）。
    // 简化：借用现有逐字符状态机逻辑，直到 CRLFCRLF 结束，然后设置 flags。
    char *start = const_cast<char*>(begin);
    char *end = start;
    char *colon = end;
    while (state_ != HttpRequestParseState::INVALID && state_ != HttpRequestParseState::COMPLETE && end - begin <= size)
    {
        char ch = *end;
        switch (state_)
        {
        case HttpRequestParseState::START:
            if (!(ch == CR || ch == LF || isblank(ch))) {
                if (isupper(ch)) { state_ = HttpRequestParseState::METHOD; start = end; } else state_ = HttpRequestParseState::INVALID; }
            break;
        case HttpRequestParseState::METHOD:
            if (isblank(ch)) { request_->SetMethod(std::string(start, end)); state_ = HttpRequestParseState::BEFORE_URL; start = end + 1; }
            break;
        case HttpRequestParseState::BEFORE_URL:
            if (ch == '/') { state_ = HttpRequestParseState::IN_URL; start = end; }
            else if (!isblank(ch)) state_ = HttpRequestParseState::INVALID; break;
        case HttpRequestParseState::IN_URL:
            if (ch == '?') { request_->SetUrl(std::string(start, end)); start = end + 1; state_ = HttpRequestParseState::BEFORE_URL_PARAM_KEY; }
            else if (isblank(ch)) { request_->SetUrl(std::string(start, end)); start = end + 1; state_ = HttpRequestParseState::BEFORE_PROTOCOL; }
            break;
        case HttpRequestParseState::BEFORE_URL_PARAM_KEY:
            if (ch == CR || ch == LF || isblank(ch)) state_ = HttpRequestParseState::INVALID; else state_ = HttpRequestParseState::URL_PARAM_KEY; break;
        case HttpRequestParseState::URL_PARAM_KEY:
            if (ch == '=') { colon = end; state_ = HttpRequestParseState::BEFORE_URL_PARAM_VALUE; }
            else if (isblank(ch)) state_ = HttpRequestParseState::INVALID; break;
        case HttpRequestParseState::BEFORE_URL_PARAM_VALUE:
            if (ch == CR || ch == LF || isblank(ch)) state_ = HttpRequestParseState::INVALID; else state_ = HttpRequestParseState::URL_PARAM_VALUE; break;
        case HttpRequestParseState::URL_PARAM_VALUE:
            if (ch == '&') { request_->SetRequestParams(std::string(start, colon), std::string(colon + 1, end)); start = end + 1; state_ = HttpRequestParseState::BEFORE_URL_PARAM_KEY; }
            else if (isblank(ch)) { request_->SetRequestParams(std::string(start, colon), std::string(colon + 1, end)); start = end + 1; state_ = HttpRequestParseState::BEFORE_PROTOCOL; }
            break;
        case HttpRequestParseState::BEFORE_PROTOCOL:
            if (!isblank(ch)) { state_ = HttpRequestParseState::PROTOCOL; start = end; } break;
        case HttpRequestParseState::PROTOCOL:
            if (ch == '/') { request_->SetProtocol(std::string(start, end)); start = end + 1; state_ = HttpRequestParseState::BEFORE_VERSION; } break;
        case HttpRequestParseState::BEFORE_VERSION:
            if (isdigit(ch)) { state_ = HttpRequestParseState::VERSION; start = end; } else state_ = HttpRequestParseState::INVALID; break;
        case HttpRequestParseState::VERSION:
            if (ch == CR) { request_->SetVersion(std::string(start, end)); start = end + 1; state_ = HttpRequestParseState::WHEN_CR; }
            else if (!(ch == '.' || isdigit(ch))) state_ = HttpRequestParseState::INVALID_VERSION; break;
        case HttpRequestParseState::WHEN_CR:
            if (ch == LF) { state_ = HttpRequestParseState::CR_LF; start = end + 1; } else state_ = HttpRequestParseState::INVALID; break;
        case HttpRequestParseState::CR_LF:
            if (ch == CR) { state_ = HttpRequestParseState::CR_LF_CR; }
            else if (isblank(ch)) state_ = HttpRequestParseState::INVALID; else state_ = HttpRequestParseState::HEADER_KEY; break;
        case HttpRequestParseState::HEADER_KEY:
            if (ch == ':') { colon = end; state_ = HttpRequestParseState::HEADER_VALUE; }
            break;
        case HttpRequestParseState::HEADER_VALUE:
            if (ch == CR) { request_->AddHeader(std::string(start, colon), std::string(colon + 2, end)); start = end + 1; state_ = HttpRequestParseState::WHEN_CR; }
            break;
        case HttpRequestParseState::CR_LF_CR:
            if (ch == LF) {
                headers_complete_ = true;
                // 判断 body 模式
                auto &hs = request_->GetHeaders();
                if (hs.count("Transfer-Encoding") && hs.at("Transfer-Encoding") == "chunked") { chunked_ = true; state_ = HttpRequestParseState::BODY; }
                else if (hs.count("Content-Length")) { content_length_ = static_cast<size_t>(::atoll(request_->GetHeader("Content-Length").c_str())); if (content_length_ > 0) state_ = HttpRequestParseState::BODY; else { body_complete_ = true; state_ = HttpRequestParseState::COMPLETE; } }
                else { // 无长度：假定无 body
                    body_complete_ = true; state_ = HttpRequestParseState::COMPLETE;
                }
            } else state_ = HttpRequestParseState::INVALID; break;
        case HttpRequestParseState::BODY:
            // 剩余全部视作 body 一次性吸收（兼容旧调用）
            {
                size_t body_len = static_cast<size_t>(begin + size - end);
                if (body_len) request_->AppendBody(end, body_len);
                received_body_bytes_ += body_len;
                if (chunked_) { body_complete_ = true; } // 旧接口不支持增量 chunk 直接标记完成
                else if (received_body_bytes_ >= content_length_) { body_complete_ = true; }
                if (body_complete_) state_ = HttpRequestParseState::COMPLETE;
                end = const_cast<char*>(begin) + size; // 跳出循环
                continue;
            }
        default:
            state_ = HttpRequestParseState::INVALID; break;
        }
        ++end;
    }
    return state_ == HttpRequestParseState::COMPLETE || state_ == HttpRequestParseState::BODY;
}

bool HttpContext::ParseRequest(const std::string &request)
{
    return ParseRequest(request.c_str(), request.size());
}
bool HttpContext::ParseRequest(const char *begin)
{
    return ParseRequest(begin, ::strlen(begin));
}

void HttpContext::StoreDeferredResponse(const HttpResponse &resp)
{
    deferred_response_ = std::make_unique<HttpResponse>(resp);
}

bool HttpContext::HasDeferredResponse() const
{
    return static_cast<bool>(deferred_response_);
}

HttpResponse *HttpContext::GetDeferredResponse()
{
    return deferred_response_.get();
}

void HttpContext::ClearDeferredResponse()
{
    deferred_response_.reset();
}

bool HttpContext::ParseIncremental(const char* data, size_t len)
{
    size_t dummy = 0; return ParseIncremental(data, len, dummy);
}

bool HttpContext::ParseIncremental(const char* data, size_t len, size_t &consumedBytes)
{
    consumedBytes = 0;
    // 若头还未完成，逐行处理
    if (!headers_complete_) {
        // 查找 CRLF 行
        while (consumedBytes < len && !headers_complete_) {
            const char* lineStart = data + consumedBytes;
            const char* cr = static_cast<const char*>(memchr(lineStart, '\r', len - consumedBytes));
            if (!cr || (cr + 1 >= data + len)) {
                // 行尚未完整
                return true; // 等待更多数据
            }
            if (*(cr + 1) != '\n') { state_ = HttpRequestParseState::INVALID; return false; }
            size_t lineLen = static_cast<size_t>(cr - lineStart); // 不含 CR
            std::string line(lineStart, lineLen);
            consumedBytes += lineLen + 2; // 跳过 CRLF
            if (state_ == HttpRequestParseState::START) state_ = HttpRequestParseState::METHOD;
            if (line.empty()) {
                // 空行 -> 头结束
                headers_complete_ = true;
                auto &hs = request_->GetHeaders();
                if (hs.count("Transfer-Encoding") && hs.at("Transfer-Encoding") == "chunked") { chunked_ = true; }
                else if (hs.count("Content-Length")) { content_length_ = static_cast<size_t>(::atoll(request_->GetHeader("Content-Length").c_str())); }
                if ((!chunked_ && content_length_ == 0)) { body_complete_ = true; state_ = HttpRequestParseState::COMPLETE; }
                break;
            }
            if (request_->GetMethod() == HttpMethod::kInvalid) {
                // 第一行: 请求行  METHOD SP URL SP HTTP/1.x
                size_t p1 = line.find(' '); if (p1 == std::string::npos) { state_ = HttpRequestParseState::INVALID; return false; }
                size_t p2 = line.find(' ', p1 + 1); if (p2 == std::string::npos) { state_ = HttpRequestParseState::INVALID; return false; }
                request_->SetMethod(line.substr(0, p1));
                request_->SetUrl(line.substr(p1 + 1, p2 - p1 - 1));
                std::string proto = line.substr(p2 + 1);
                // proto 形如 HTTP/1.1
                if (proto.rfind("HTTP/", 0) == 0) request_->SetVersion(proto.substr(5));
            } else {
                // 头行 key: value
                size_t colon = line.find(':');
                if (colon == std::string::npos) { state_ = HttpRequestParseState::INVALID; return false; }
                size_t vstart = colon + 1;
                while (vstart < line.size() && (line[vstart] == ' ' || line[vstart] == '\t')) ++vstart;
                std::string key = line.substr(0, colon);
                std::string value = line.substr(vstart);
                request_->AddHeader(key, value);
            }
        }
    }
    if (headers_complete_ && !body_complete_) {
        size_t bodyConsumed = 0;
        size_t remain = len - consumedBytes;
        if (chunked_) {
            if (!ParseChunkedBlock(data + consumedBytes, remain, bodyConsumed)) return false;
        } else if (content_length_ > 0) {
            if (!ParseBodyBlock(data + consumedBytes, remain, bodyConsumed)) return false;
        }
        consumedBytes += bodyConsumed;
    }
    return true;
}

bool HttpContext::ParseBodyBlock(const char* data, size_t len, size_t &consumed)
{
    size_t need = content_length_ - received_body_bytes_;
    size_t take = std::min(need, len);
    if (take) {
        request_->AppendBody(data, take);
        received_body_bytes_ += take;
    }
    consumed = take;
    if (received_body_bytes_ >= content_length_) { body_complete_ = true; state_ = HttpRequestParseState::COMPLETE; }
    return true;
}

bool HttpContext::ParseChunkedBlock(const char* data, size_t len, size_t &consumed)
{
    consumed = 0;
    while (consumed < len && !body_complete_) {
        switch (chunk_state_) {
        case ChunkState::SIZE:
            if (data[consumed] == '\r') { chunk_state_ = ChunkState::SIZE_CR; }
            else { chunk_size_buf_.push_back(data[consumed]); }
            ++consumed; break;
        case ChunkState::SIZE_CR:
            if (data[consumed] != '\n') return false; // 协议错误
            // 解析十六进制块大小
            current_chunk_size_ = strtoul(chunk_size_buf_.c_str(), nullptr, 16);
            chunk_size_buf_.clear();
            if (current_chunk_size_ == 0) { chunk_state_ = ChunkState::TRAILERS; }
            else { chunk_state_ = ChunkState::DATA; }
            ++consumed; break;
        case ChunkState::DATA: {
            size_t remain = current_chunk_size_;
            size_t take = std::min(remain, len - consumed);
            request_->AppendBody(data + consumed, take);
            current_chunk_size_ -= take;
            consumed += take;
            if (current_chunk_size_ == 0) chunk_state_ = ChunkState::DATA_CR;
            break; }
        case ChunkState::DATA_CR:
            if (data[consumed] != '\r') return false; ++consumed; chunk_state_ = ChunkState::DATA_LF; break;
        case ChunkState::DATA_LF:
            if (data[consumed] != '\n') return false; ++consumed; chunk_state_ = ChunkState::SIZE; break;
        case ChunkState::TRAILERS:
            // 读到 CRLF 结束（忽略实际 trailer 内容）
            if (data[consumed] == '\r') { chunk_state_ = ChunkState::COMPLETE; }
            ++consumed; break;
        case ChunkState::COMPLETE:
            if (data[consumed] == '\n') { body_complete_ = true; state_ = HttpRequestParseState::COMPLETE; ++consumed; }
            else return false; break;
        }
    }
    return true;
}