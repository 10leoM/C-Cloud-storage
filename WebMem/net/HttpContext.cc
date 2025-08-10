#include "HttpContext.h"
#include <algorithm>
#include <string.h>
#include "base/Timestamp.h"
#include "base/Logging.h"
using namespace mymuduo;
using namespace mymuduo::net;
bool HttpContext::processRequestLine(const char* begin, const char* end)
{
  bool succeed = false;
  const char* start = begin;
  const char* space = std::find(start, end, ' ');
  if (space != end && request_.setMethod(start, space))
  {
    start = space+1;
    space = std::find(start, end, ' ');
    if (space != end)
    {
      const char* question = std::find(start, space, '?');
      if (question != space)
      {
        request_.setPath(start, question);
        request_.setQuery(question, space);
      }
      else
      {
        request_.setPath(start, space);
      }
      start = space+1;
      succeed = end-start == 8 && std::equal(start, end-1, "HTTP/1.");
      if (succeed)
      {
        if (*(end-1) == '1')
        {
          request_.setVersion(HttpRequest::kHttp11);
        }
        else if (*(end-1) == '0')
        {
          request_.setVersion(HttpRequest::kHttp10);
        }
        else
        {
          succeed = false;
        }
      }
    }
  }
  return succeed;
}

bool HttpContext::processHeaders(Buffer* buf) {
    bool ok = true;
    bool hasMore = true;
    while (hasMore) {
        const char* crlf = buf->findCRLF();
        if (crlf) {
            const char* colon = std::find(buf->peek(), crlf, ':');
            if (colon != crlf) {
                request_.addHeader(buf->peek(), colon, crlf);
                
                // 检查是否有 Content-Length 头
                std::string field(buf->peek(), colon);
                if (strcasecmp(field.c_str(), "Content-Length") == 0) {
                    ++colon;  // 跳过冒号
                    while (colon < crlf && isspace(*colon)) {
                        ++colon;  // 跳过空白字符
                    }
                    contentLength_ = static_cast<size_t>(atoi(colon));
                    LOG_INFO << "Content-Length: " << contentLength_;
                } else if (strcasecmp(field.c_str(), "Transfer-Encoding") == 0) {
                    ++colon;
                    while (colon < crlf && isspace(*colon)) {
                        ++colon;
                    }
                    std::string encoding(colon, crlf);
                    if (strcasecmp(encoding.c_str(), "chunked") == 0) {
                        isChunked_ = true;
                        LOG_INFO << "Transfer-Encoding: chunked";
                    }
                }
                buf->retrieveUntil(crlf + 2);
            } else {
                // 空行，头部解析完成
                buf->retrieveUntil(crlf + 2);
                state_ = kExpectBody;
                hasMore = false;
            }
        } else {
            hasMore = false;
        }
    }
    return ok;
}

bool HttpContext::processBody(Buffer* buf) {
    if (isChunked_) {
        // TODO: 处理 chunked 传输
        return false;
    } else {
        size_t readable = buf->readableBytes();
        LOG_DEBUG << "processBody readable: " << readable;
        size_t toRead = std::min(readable, remainingLength());
        if (toRead > 0) {
            request_.appendToBody(buf->peek(), toRead);
            bodyReceived_ += toRead;
            buf->retrieve(toRead);
        }
        LOG_DEBUG << "bodyReceived_: " << bodyReceived_ << ", contentLength_: " << contentLength_;
        return bodyReceived_ >= contentLength_;
    }
}

// return false for error, true for success (got all or need more data)
HttpContext::ParseResult HttpContext::parseRequest(Buffer* buf, Timestamp receiveTime) {
    bool ok = true;
    bool hasMore = true;
    ParseResult result = kNeedMore;

    LOG_DEBUG << "parseRequest state_: " << state_ << ", result: " << result;
    LOG_DEBUG << "buf: " << buf->peek();
    
    while (hasMore) {
        if (state_ == kExpectRequestLine) {
            const char* crlf = buf->findCRLF();
            if (crlf) {
                ok = processRequestLine(buf->peek(), crlf);
                if (ok) {
                    request_.setReceiveTime(receiveTime);
                    buf->retrieveUntil(crlf + 2);
                    state_ = kExpectHeaders;
                } else {
                    result = kError;
                    hasMore = false;
                }
            } else {
                hasMore = false;
            }
        } else if (state_ == kExpectHeaders) {
            ok = processHeaders(buf);
            if (ok) {
                if (state_ == kExpectBody) {
                    if (contentLength_ == 0 && !isChunked_) {
                        // 没有请求体
                        state_ = kGotAll;
                        result = kGotRequest;
                        hasMore = false;
                    } else {
                        // 有请求体，继续处理
                        result = kHeadersComplete;
                        if (buf->readableBytes() > 0) {
                            // 如果缓冲区还有数据，继续处理body
                            continue;
                        }
                        hasMore = false;
                    }
                }
            } else {
                result = kError;
                hasMore = false;
            }
        } else if (state_ == kExpectBody) {
            // 处理请求体
            if (processBody(buf)) {
                state_ = kGotAll;
                result = kGotRequest;
            } else if (bodyReceived_ < contentLength_) {
                // 还需要更多数据
                result = kHeadersComplete;
            }
            hasMore = false;
        }
    }

    return result;  // 只要不是错误状态都返回true
}