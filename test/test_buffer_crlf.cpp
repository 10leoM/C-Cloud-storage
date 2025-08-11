#include "Buffer.h"
#include <cassert>
#include <iostream>
#include <vector>

// 简单单元/集成测试：构造多行含CRLF数据，验证 findCRLF / RetrieveUtil
int main() {
    Buffer buf;
    std::string data = "GET /index.html HTTP/1.1\r\nHost: example.com\r\nUser-Agent: test-agent\r\nAccept: */*\r\n\r\nBODY";
    buf.Append(data);

    std::vector<std::string> lines;
    const char *start = buf.Peek();
    while (true) {
        const char *crlf = buf.findCRLF(start);
        if (!crlf) break;
        std::string line(start, crlf);
        lines.push_back(line);
        // Retrieve 这一行（包含CRLF）
        buf.RetrieveUtil(crlf + 2); // +2 跳过 CRLF
        start = buf.Peek();
        if (buf.GetReadablebytes() == 0) break;
    }

    // 断言前4行（空行前）
    assert(lines.size() == 5); // 包含空行（第四行为空字符串）以及起始请求行
    assert(lines[0].find("GET /index.html HTTP/1.1") == 0);
    assert(lines[1].find("Host: example.com") == 0);
    assert(lines[2].find("User-Agent: test-agent") == 0);
    assert(lines[3].find("Accept: */*") == 0);
    assert(lines[4].empty()); // 空行

    // 剩余部分应该是 BODY
    std::string remaining = buf.RetrieveAllAsString();
    assert(remaining == "BODY");

    std::cout << "test_buffer_crlf PASS" << std::endl;
    return 0;
}
