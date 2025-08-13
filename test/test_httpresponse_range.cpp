#include "HttpResponse.h"
#include "Buffer.h"
#include <cassert>
#include <iostream>

int main() {
    // 构造部分内容响应
    HttpResponse resp(false);
    resp.SetStatusCode(HttpStatusCode::OK); // 允许自动改写为206
    resp.SetStatusMessage("OK");
    resp.SetBodyType(HttpBodyType::HTML_TYPE); // 仅测试头部生成逻辑
    resp.SetContentType("text/plain");
    bool ok = resp.SetContentRange(10, 19, 100); // 10 bytes
    assert(ok);
    Buffer buf;
    resp.AppendToBuffer(&buf);
    std::string out = buf.PeekAllAsString();
    // 必须包含206 与 Content-Range
    assert(out.find("206") != std::string::npos);
    assert(out.find("Content-Range: bytes 10-19/100") != std::string::npos);
    assert(out.find("Content-Length: 10") != std::string::npos);
    std::cout << "test_httpresponse_range passed" << std::endl;
    return 0;
}
