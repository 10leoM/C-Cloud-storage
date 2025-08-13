#include "HttpContext.h"
#include "HttpRequest.h"
#include <cassert>
#include <iostream>

int main() {
    // 头部行长度限制
    {
        HttpContext ctx;
        HttpLimits lim; lim.max_header_line_len = 10; // 极小
        ctx.SetLimits(lim);
        std::string req = "GET /a HTTP/1.1\r\nHost: abcdefghijklmnopqrstuvwxyz\r\n\r\n";
        size_t c = 0;
        bool ok = ctx.ParseIncremental(req.data(), req.size(), c);
        assert(!ok && "Header line too long should fail");
    }
    // 头部总字节限制
    {
        HttpContext ctx;
        HttpLimits lim; lim.max_header_bytes = 20; // 极小
        ctx.SetLimits(lim);
        std::string req = "GET /a HTTP/1.1\r\nHost: x\r\nUser-Agent: y\r\n\r\n";
        size_t c = 0;
        bool ok = ctx.ParseIncremental(req.data(), req.size(), c);
        assert(!ok && "Header bytes too large should fail");
    }
    // 请求体限制
    {
        HttpContext ctx;
        HttpLimits lim; lim.max_body_bytes = 5;
        ctx.SetLimits(lim);
        std::string req = "POST /a HTTP/1.1\r\nContent-Length: 10\r\n\r\n1234567890";
        size_t c = 0;
        bool ok = ctx.ParseIncremental(req.data(), req.size(), c);
        assert(!ok && "Body too large should fail");
    }
    // Range 头解析
    {
        HttpRequest req;
        req.AddHeader("Range", "bytes=100-200");
        assert(req.ParseRangeHeader());
        assert(req.HasRange());
        assert(req.GetRangeStart() == 100);
        assert(req.GetRangeEnd() == 200);
        assert(!req.IsRangeSuffix());
    }
    {
        HttpRequest req;
        req.AddHeader("Range", "bytes=-500");
        assert(req.ParseRangeHeader());
        assert(req.HasRange());
        assert(req.IsRangeSuffix());
        assert(req.GetRangeEnd() == 500);
    }
    {
        HttpRequest req;
        req.AddHeader("Range", "bytes=100-");
        assert(req.ParseRangeHeader());
        assert(req.HasRange());
        assert(req.GetRangeStart() == 100);
        assert(req.GetRangeEnd() == -1);
    }
    std::cout << "test_range_limit passed" << std::endl;
    return 0;
}
