#include "EventLoop.h"
#include "Server.h"
#include "Connection.h"
#include <string.h>

int main() {
    EventLoop *loop = new EventLoop(); // 创建事件循环
    Server *server = new Server(loop);
    server->SetThreadPoolSize(std::thread::hardware_concurrency()); // 设置线程池大小

    server->setMessageCallback([](std::shared_ptr<Connection> const &ncon){
        if(ncon->GetState() == connectionState::Closed)
        {
            // ncon->Close(); // 这里不需要手动关闭连接，已经在Connection的析构函数中处理了
            return;
        }
        std::string message = ncon->GetReadBuffer()->PeekAllAsString(); // 获取接收到的消息
        std::cout<< "Message from client " << ncon->GetFd() << ": " << message << std::endl;
        for(char &c : message) {
            c = toupper(c); // 将接收到的消息转换为大写
        }
        ncon->Send(message); // 发送转换后的数据
        if(ncon->GetState() == connectionState::Closed)
            return;
    });
    server->start(); // 启动服务器，开始监听连接

    delete server; // 删除服务器对象
    delete loop;
    return 0;
}