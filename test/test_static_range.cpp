#include "HttpServer.h"
#include "EventLoop.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include <fcntl.h>
#include <unistd.h>
#include <cassert>
#include <fstream>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>

// 仅做最小集成测试：启动服务器 -> 创建文件 -> 发送 Range 请求 -> 校验 206 及长度
int main(){
    // 准备测试文件
    system("mkdir -p files");
    std::string path = "files/testdata.bin";
    std::ofstream ofs(path, std::ios::binary);
    for(int i=0;i<100;i++) ofs.put(static_cast<char>(i));
    ofs.close();

    EventLoop loop;
    HttpServer server(&loop, "0.0.0.0", 9099, false);
    server.start();
    std::thread th([&loop]{ loop.loop(); });
    // 等待服务器启动
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    assert(fd>=0);
    sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons(9099); inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    assert(::connect(fd,(sockaddr*)&addr,sizeof(addr))==0);
    std::string req = "GET /testdata.bin HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nRange: bytes=10-19\r\n\r\n";
    ::send(fd, req.data(), req.size(), 0);
    std::string resp; char buf[1024];
    while (true) {
        int n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break; resp.append(buf, n);
        if (resp.find("\r\n\r\n") != std::string::npos && resp.find("Content-Length:") != std::string::npos) {
            // 若已读取到头部且长度足够可提前退出（简单场景）
            if (resp.find("206 Partial Content") != std::string::npos) break;
        }
    }
    // 简单断言
    if (resp.find("206 Partial Content") == std::string::npos) { std::cerr << "Response: \n" << resp; assert(false); }
    if (resp.find("Content-Range: bytes 10-19/100") == std::string::npos) { std::cerr<<resp; assert(false); }
    // 关闭
    ::close(fd);
    // 无显式 Quit 接口，直接关闭后分离线程（演示用，实际应扩展退出机制）
    // 简单方式：分离线程（测试进程结束自动退出）
    th.detach();
    std::cout << "test_static_range passed" << std::endl;
    return 0; // 进程结束后后台线程随之结束
}
