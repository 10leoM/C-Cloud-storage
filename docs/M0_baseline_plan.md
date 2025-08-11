P0 必须增强（底座 + HTTP）
1. 回调体系补齐  
   - Channel: closeCallback / errorCallback / disableAll / disableReading  
   - Connection: writeCompleteCallback / highWaterMarkCallback / closeCallback / error处理 / shutdown()/forceClose() / 半关闭状态  
   - 分离 Connection 与 HTTP（改为通用 context: std::shared_ptr<void>）
2. Acceptor / Server  
   - newConnection 传 localAddr & peerAddr  
   - 移除 Acceptor::Connect，若需要客户端做独立 Connector/TcpClient
3. Buffer  
   - 改用 size_t 索引  
   - 实现 readFd(writev) / prependXxx(int32 等) / findCRLF / retrieveUntil  
   - 高水位阈值配合回调
4. HTTP 层  
   - HttpServer: 回调签名改 bool(Conn, Request&, Response*) 支持同步/异步  
   - HttpContext: 状态机扩展 (headersComplete / gotAll / isChunked / remainingLength) + 可挂自定义子 context  
   - HttpRequest: 增强路径参数 setPathParams/getPathParam + Query 解析 + urlDecode  
   - HttpResponse: 支持 async 标记、Content-Range / Accept-Ranges、appendToBuffer统一输出  
5. 路由  
   - 引入 RouteTrie 或正则路由表 + 动态参数注入  
6. 错误 / 关闭处理  
   - 在 Epoll 轮询中检测 EPOLLERR / EPOLLHUP / EPOLLRDHUP 并映射到 Channel error/close 回调
7. 定时/任务  
   -（可复用已有）补 session/上传超时需要的定时接口（RunAfter / RunEvery）