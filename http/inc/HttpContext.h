#pragma once

#include <string>
#include <memory>
#include "HttpResponse.h" // 需要完整类型存储 unique_ptr

#define CR '\r' // 回车
#define LF '\n' // 换行

enum HttpRequestParseState // 解析状态机
{
    INVALID,         // 无效
    INVALID_METHOD,  // 无效请求方法
    INVALID_URL,     // 无效请求路径
    INVALID_VERSION, // 无效的协议版本号
    INVALID_HEADER,  // 无效请求头

    START,  // 解析开始
    METHOD, // 请求方法

    BEFORE_URL, // 请求连接前的状态，需要'/'开头
    IN_URL,     // url处理

    BEFORE_URL_PARAM_KEY,   // URL请求参数键之前
    URL_PARAM_KEY,          // URL请求参数键
    BEFORE_URL_PARAM_VALUE, // URL请求参数值之前
    URL_PARAM_VALUE,        // URL请求参数值

    BEFORE_PROTOCOL, // 协议解析之前
    PROTOCOL,        // 协议

    BEFORE_VERSION, // 版本开始前
    VERSION_SPLIT,  // 版本分割符
    VERSION,        // 版本

    HEADER,
    HEADER_KEY, //

    HEADER_BEFORE_COLON, // 请求头冒号之前
    HEADER_AFTER_COLON,  // 请求头冒号
    HEADER_VALUE,        // 请求值

    WHEN_CR, // 遇到一个回车

    CR_LF, // 回车换行

    CR_LF_CR, // 回车换行之后的状态

    BODY, // 请求体

    COMPLETE, // 完成
};

class HttpRequest; // 前向声明

struct HttpLimits {
    size_t max_header_line_len = 8192;    // 单行最大长度
    size_t max_header_bytes = 16384;      // 头部总字节数
    size_t max_body_bytes = 104857600;    // 最大请求体（100MB）
};

class HttpContext
{
private:
    std::shared_ptr<HttpRequest> request_; // 请求对象
    HttpRequestParseState state_;          // 当前解析状态

    // 异步场景: 业务回调返回 false 表示暂不发送响应，将 HttpResponse 临时保存
    std::unique_ptr<HttpResponse> deferred_response_;

    // 扩展: 头部完成/主体完成标记与内容控制
    bool headers_complete_ = false;  // 请求头是否完成
    bool body_complete_ = false;     // 请求体是否完成
    bool chunked_ = false;           // 是否是 chunked 编码
    size_t content_length_ = 0;      // 期望的 Content-Length 剩余未读长度
    size_t received_body_bytes_ = 0; // 已累计正文字节
    size_t header_bytes_ = 0;        // 已累计头部字节数
    HttpLimits limits_ = {};         // 限制配置

    // chunked 解析临时字段
    enum class ChunkState
    {
        SIZE,       // 解析块大小
        SIZE_CR,    // 解析块大小后的回车
        DATA,       // 数据块
        DATA_CR,    // 数据块后的回车
        DATA_LF,    // 数据块后的换行
        TRAILERS,   // 解析 trailer
        COMPLETE    // 完成
    };
    ChunkState chunk_state_ = ChunkState::SIZE;
    size_t current_chunk_size_ = 0; // 当前块剩余字节
    std::string chunk_size_buf_;    // 块大小行缓冲（十六进制）

public:
    HttpContext();
    ~HttpContext();
    void SetLimits(const HttpLimits& lim) { limits_ = lim; }
    const HttpLimits& GetLimits() const { return limits_; }

    bool ParseRequest(const char *begin, int size); // 解析请求
    bool ParseRequest(const std::string &request);  // 解析请求字符串
    bool ParseRequest(const char *begin);           // 解析请求起始位置
    bool GetCompleteRequest();                      // 获取完整请求
    HttpRequest *GetRequest();                      // 获取请求对象
    void ResetContextStatus();                      // 重置上下文状态

    // 异步响应管理
    void StoreDeferredResponse(const HttpResponse &resp); // 保存一份待发送响应
    bool HasDeferredResponse() const;                     // 是否存在待发送响应
    HttpResponse *GetDeferredResponse();                  // 取得待发送响应指针
    void ClearDeferredResponse();                         // 清除待发送响应

    // 新增状态查询
    bool HeadersComplete() const { return headers_complete_; }
    bool BodyComplete() const { return body_complete_; }
    bool IsChunked() const { return chunked_; }
    size_t RemainingContentLength() const { return chunked_ ? 0 : content_length_ - received_body_bytes_; }

    // 增量解析入口（供上层按分片调用）
    // 返回: true 表示解析推进正常; false 表示非法/出错
    bool ParseIncremental(const char *data, size_t len);
    // 细化: 返回实际消费字节（便于上层从 Buffer Retrieve）
    bool ParseIncremental(const char *data, size_t len, size_t &consumedBytes);
    // 内部使用: 解析已获取 Header 后的正文（Content-Length 模式）
    bool ParseBodyBlock(const char *data, size_t len, size_t &consumed);
    // 内部使用: chunked 编码解析
    bool ParseChunkedBlock(const char *data, size_t len, size_t &consumed);
};