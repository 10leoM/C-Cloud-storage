#include "HttpContext.h"
#include "HttpRequest.h"
#include <cassert>
#include <iostream>

int main(){
    // 测试: 增量解析 + 分块传输
    {
        HttpContext ctx; std::string p1="GET /a/b?x=1&x=2 HTTP/1.1\r\nHost: example.com\r\nCon";
        std::string p2="nection: keep-alive\r\nContent-Length: 5\r\n\r\n"; std::string p3="HelloEXTRA";
        size_t c=0; ctx.ParseIncremental(p1.data(), p1.size(), c); // 可能未消费最后半行
        assert(c < p1.size());
        ctx.ParseIncremental(p2.data(), p2.size(), c); assert(c==p2.size());
        assert(ctx.HeadersComplete()); assert(!ctx.BodyComplete());
        ctx.ParseIncremental(p3.data(), 5, c); assert(c==5); assert(ctx.BodyComplete());
        auto *req=ctx.GetRequest(); assert(req->GetBody()=="Hello");
        assert(req->GetQueryValues("x").size()==2);
    }
    // 测试: 增量解析 + 分块传输
    {
        HttpContext ctx; std::string head="POST /upload HTTP/1.1\r\nHost: h\r\nTransfer-Encoding: chunked\r\n\r\n";
        size_t c=0; ctx.ParseIncremental(head.data(), head.size(), c); assert(c==head.size());
        std::string c1="4\r\nWiki\r\n"; ctx.ParseIncremental(c1.data(), c1.size(), c); assert(c==c1.size());
        std::string c2="5\r\npedia\r\n"; ctx.ParseIncremental(c2.data(), c2.size(), c); assert(c==c2.size());
        std::string last="0\r\n\r\n"; ctx.ParseIncremental(last.data(), last.size(), c); assert(c==last.size());
        assert(ctx.BodyComplete()); auto *req=ctx.GetRequest(); assert(req->GetBody()=="Wikipedia");
    }
    std::cout<<"testhttpincremental passed"<<std::endl; return 0; }
