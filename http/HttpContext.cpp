#include "HttpContext.h"
#include "HttpRequest.h"
#include <string.h>

HttpContext::HttpContext() : state_(HttpRequestParseState::START)
{
    request_ = std::make_shared<HttpRequest>();
}

HttpContext::~HttpContext() {}

bool HttpContext::GetCompleteRequest()
{
    return state_ == HttpRequestParseState::COMPLETE;
}

void HttpContext::ResetContextStatus()
{
    state_ = HttpRequestParseState::START;
}

HttpRequest *HttpContext::GetRequest()
{
    return request_.get();
}

bool HttpContext::ParseRequest(const char *begin, int size)
{
    char *start = static_cast<char *>(const_cast<char *>(begin));
    char *end = start;
    char *colon = end; // 用于URL参数和请求头的中间位置

    // 逐字符解析请求
    while (state_ != HttpRequestParseState::INVALID && state_ != HttpRequestParseState::COMPLETE && end - begin <= size)
    {
        char ch = *end; // 当前字符
        switch (state_)
        {
        case HttpRequestParseState::START:
        {
            if (ch == CR || ch == LF || isblank(ch))
            {
                // 遇到空格、换行或回车，继续解析
            }
            else if (isupper(ch))
            {
                // 遇到大写字母，说明遇到了METHOD
                state_ = HttpRequestParseState::METHOD;
                start = end; // 更新start位置
            }
            else
            {
                state_ = HttpRequestParseState::INVALID;
            }
            break;
        }

        case HttpRequestParseState::METHOD:
        {
            if (isupper(ch))
            {
                // 如果是大写字母，则继续
            }
            else if (isblank(ch))
            {
                // 遇到空格表明，METHOD方法解析结束，当前处于即将解析URL
                request_->SetMethod(std::string(start, end));
                state_ = HttpRequestParseState::BEFORE_URL;
                start = end + 1; // 更新下一个指标的位置
            }
            break;
        }

        case HttpRequestParseState::BEFORE_URL:
        {
            // 对请求连接前的处理，请求连接以'/'开头
            if (ch == '/')
            {
                // 遇到/ 说明遇到了URL，开始解析
                state_ = HttpRequestParseState::IN_URL;
                start = end; // 更新start位置
            }
            else if (isblank(ch))
            {
                // 遇到空格，继续
            }
            else
            {
                state_ = HttpRequestParseState::INVALID;
            }
            break;
        }

        case HttpRequestParseState::IN_URL:
        {
            // 进入url中
            if (ch == '?')
            {
                // 遇到参数解析
                request_->SetUrl(std::string(start, end));
                start = end + 1; // 更新start位置
                state_ = HttpRequestParseState::BEFORE_URL_PARAM_KEY;
            }
            else if (isblank(ch))
            {
                // url解析结束
                request_->SetUrl(std::string(start, end));
                start = end + 1; // 更新start位置
                state_ = HttpRequestParseState::BEFORE_PROTOCOL;
            }
            break;
        }

        case HttpRequestParseState::BEFORE_URL_PARAM_KEY:
        {
            if (ch == CR || ch == LF || isblank(ch))
            {
                // 当开始进入url params时，遇到了空格，换行等，则不合法
                state_ = HttpRequestParseState::INVALID;
            }
            else
            {
                state_ = HttpRequestParseState::URL_PARAM_KEY;
            }
            break;
        }

        case HttpRequestParseState::URL_PARAM_KEY:
        {
            if (ch == '=')
            {
                // 遇到= 说明一个key解析完成
                colon = end;
                state_ = HttpRequestParseState::BEFORE_URL_PARAM_VALUE;
            }
            else if (isblank(ch))
            {
                state_ = HttpRequestParseState::INVALID;
            }
            break;
        }

        case HttpRequestParseState::BEFORE_URL_PARAM_VALUE:
        {
            if (ch == CR || ch == LF || isblank(ch))
            {
                // 当开始进入url params时，遇到了空格，换行等，则不合法
                state_ = HttpRequestParseState::INVALID;
            }
            else
            {
                // 进入URL参数值的解析
                state_ = HttpRequestParseState::URL_PARAM_VALUE;
            }
        }

        case HttpRequestParseState::URL_PARAM_VALUE:
        {
            if (ch == '&')
            {
                // 遇到& 说明一个URL参数解析完成，还有下一个参数
                state_ = HttpRequestParseState::BEFORE_URL_PARAM_KEY;
                request_->SetRequestParams(std::string(start, colon), std::string(colon + 1, end));
                start = end + 1; // 更新start位置
            }
            else if (isblank(ch))
            {
                // 遇到空格，说明URL参数解析结束
                state_ = HttpRequestParseState::BEFORE_PROTOCOL;
                request_->SetRequestParams(std::string(start, colon), std::string(colon + 1, end));
                start = end + 1; // 更新start位置
            }
            break;
        }

        case HttpRequestParseState::BEFORE_PROTOCOL:
        {
            if (isblank(ch))
            {
                // 继续
            }
            else
            {
                state_ = HttpRequestParseState::PROTOCOL;
                start = end; // 更新start位置
            }
            break;
        }

        case HttpRequestParseState::PROTOCOL:
        {
            if (ch == '/')
            {
                // 遇到/ 说明协议解析结束
                request_->SetProtocol(std::string(start, end));
                start = end + 1; // 更新start位置
                state_ = HttpRequestParseState::BEFORE_VERSION;
            }
            else
            {
                // 继续
            }
            break;
        }

        case HttpRequestParseState::BEFORE_VERSION:
        {
            if (isdigit(ch))
            {
                // 遇到数字，说明版本开始
                state_ = HttpRequestParseState::VERSION;
                start = end; // 更新start位置
            }
            else
            {
                state_ = HttpRequestParseState::INVALID;
            }
        }

        case HttpRequestParseState::VERSION:
        {
            if (ch == CR)
            {
                // 遇到回车或换行，说明版本解析结束
                request_->SetVersion(std::string(start, end));
                start = end + 1; // 更新start位置
                state_ = HttpRequestParseState::WHEN_CR;
            }
            else if (ch == '.' || isdigit(ch))
            {
                // 遇到数字或. 说明版本号解析中
            }
            else
            {
                state_ = HttpRequestParseState::INVALID_VERSION;
            }
            break;
        }

        case HttpRequestParseState::WHEN_CR:
        {
            if (ch == LF)
            {
                // 遇到换行，说明该行结束
                state_ = HttpRequestParseState::CR_LF;
                start = end + 1; // 更新start位置
            }
            else
            {
                // 遇到其他字符，说明无效请求
                state_ = HttpRequestParseState::INVALID;
            }
            break;
        }

        case HttpRequestParseState::CR_LF:
        {
            // std::cout << "111" << ch << std::endl;
            if (ch == CR)
            {
                // 说明遇到了空行，大概率时结束了
                state_ = HttpRequestParseState::CR_LF_CR;
                // start  = end + 1;
                // std::cout << "a:" << (*start == '\n') << std::endl;
                // std::cout << "b:" << (*end == '\r') << std::endl;
            }
            else if (isblank(ch))
            {
                state_ = HttpRequestParseState::INVALID;
            }
            else
            {
                state_ = HttpRequestParseState::HEADER_KEY;
            }
            break;
        }

        // 需要注意的是，对header的解析并不鲁棒
        case HttpRequestParseState::HEADER_KEY:
        {
            if (ch == ':')
            {
                colon = end;
                state_ = HttpRequestParseState::HEADER_VALUE;
            }
            break;
        }

        case HttpRequestParseState::HEADER_VALUE:
        {
            if (isblank(ch))
            {
                // 继续解析
            }
            else if (ch == CR)
            {
                request_->AddHeader(std::string(start, colon), std::string(colon + 2, end));
                start = end + 1;
                state_ = HttpRequestParseState::WHEN_CR;
            }
            break;
        }

        case HttpRequestParseState::CR_LF_CR:
        {
            // 判断是否需要解析请求体
            //
            // std::cout << "c:" << (ch == '\n') << std::endl;
            // std::cout << "size:" << end-begin << std::endl;
            if (ch == LF)
            {
                // 这就意味着遇到了空行，要进行解析请求体了
                if (request_->GetHeaders().count("Content-Length"))
                {
                    if (atoi(request_->GetHeader("Content-Length").c_str()) > 0)
                    {
                        state_ = HttpRequestParseState::BODY;
                    }
                    else
                    {
                        state_ = HttpRequestParseState::COMPLETE;
                    }
                }
                else
                {
                    if (end - begin < size)
                    {
                        state_ = HttpRequestParseState::BODY;
                    }
                    else
                    {
                        state_ = HttpRequestParseState::COMPLETE;
                    }
                }
                start = end + 1;
            }
            else
            {
                state_ = HttpRequestParseState::INVALID;
            }
            break;
        }

        case HttpRequestParseState::BODY:
        {
            int bodylength = size - (end - begin);
            // std::cout << "bodylength:" << bodylength << std::endl;
            request_->SetBody(std::string(start, start + bodylength));
            if(bodylength >= atoi(request_->GetHeader("Content-Length").c_str()))
            {
                state_ = HttpRequestParseState::COMPLETE;
            }
            break;
        }

        default:
        {
            state_ = HttpRequestParseState::INVALID;
            break;
        }
        }
        end++; // 移动到下一个字符
    }
    return state_ == HttpRequestParseState::COMPLETE || state_ == HttpRequestParseState::BODY;
}

bool HttpContext::ParseRequest(const std::string &request)
{
    return ParseRequest(request.c_str(), request.size());
}
bool HttpContext::ParseRequest(const char *begin)
{
    return ParseRequest(begin, strlen(begin));
}