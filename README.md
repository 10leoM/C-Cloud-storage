# C++ 高性能网络库与文件服务（基于 Epoll 的多 Reactor）

一个用 C++ 从零实现的轻量级网络库与示例 HTTP 文件服务，包含：多 Reactor 事件循环（主从 Reactor）、非阻塞 I/O、连接与 Channel 抽象、HTTP 协议解析、静态资源服务、文件上传/下载、分享链接、用户注册/登录等。

## 功能特性

- 网络内核
  - Epoll + 非阻塞 IO（LT/ET 可配）
  - One loop per thread，多 Reactor（主 Reactor 负责 accept，子 Reactor 负责连接 I/O）
  - 线程安全任务调度（runOneFunc/queueOneFunc + eventfd 唤醒）
  - Connection/Channel/Buffer 抽象，支持高水位回调、写完成回调
- HTTP 子系统
  - 增量解析请求（Headers/Body），支持 Range/HEAD
  - 静态资源服务（/、/index.html、/register.html、/static/*、/favicon.ico）
  - 上传（multipart/form-data，流式落盘，避免大内存占用）
  - 下载（支持 Range 分块/断点续传）
  - 分享：创建/访问/按提取码下载/查询信息
  - 简单用户体系：注册/登录/登出与基础会话校验
- 日志 & 其他
  - 同步日志接口（包含 AsyncLogging 模块，按需启用）
  - 简易文件名映射 JSON（保存原始文件名与服务端文件名）

## 目录结构（节选）

- `mymuduo`：重构的muduo网络库
  - `mymuduo/tcp/`：Reactor、EventLoop、Epoll、Channel、Connection 等
  - `mymuduo/log/`：日志类
  - `mymuduo/timer/`：定时器类
  - `mymuduo/base/`：包装一些通用工具，如线程ID获取
  - `mymuduo/router/`：字典树实现的路由
- `http/`：HTTP 协议解析与服务器封装
- `application/src/`：业务处理器
  - `StaticHandler` 静态页
  - `FileHandler` 上传/下载/列表
  - `ShareHandler` 分享
  - `AuthHandler` 注册/登录
  - `Router` 路由分发
- `application/static/`：前端页面（index.html、register.html、share.html、静态资源）
- `application/bin/http_upload`：示例 HTTP 服务可执行文件（构建后生成）

## 环境依赖

- Linux（已在本地回环与多线程环境下验证）
- CMake ≥ 3.10，GCC/G++ ≥ 9
- 可选：MySQL 客户端开发包（用于注册/登录等数据持久化）
  - Debian/Ubuntu：`sudo apt-get install libmysqlclient-dev`
  - 无数据库也可启动服务，但注册/登录等会不可用或退化

## 构建

```bash
# 在仓库根目录
mkdir build
cmake ..
make
```

生成的可执行文件位于：`application/bin/http_upload`，测试文件位于`test/bin`

## 运行

```bash
# 在仓库根目录
./application/bin/http_upload
```

默认监听：`0.0.0.0:8080`

如需调整线程数/端口，可修改 `application/http_upload.cpp` 中对应代码（例如线程数使用 `std::thread::hardware_concurrency()`，端口默认为 8080）。

## 基本使用

- 访问首页
  - 浏览器打开 `http://127.0.0.1:8080/`
  - 首页提供上传/列表/下载/分享 UI
- 注册/登录
  - 注册页：`GET /register.html`
  - 注册接口：`POST /register`（JSON：username/email/password）
  - 登录接口：`POST /login`（JSON：username/password）
- 上传文件
  - 表单方式：`POST /upload`（`multipart/form-data`），服务端流式落盘，超大文件不会占用大内存
- 文件列表
  - `GET /files` 返回 JSON 列表（在页面中用于渲染）
- 下载文件
  - `GET /download/{server_filename}`（支持 Range/HEAD）
- 分享
  - 创建：`POST /share`（页面内表单发起）
  - 访问页：`GET /share/{code}`（返回分享页面）
  - 下载：`GET /share/download/{server_filename}?code=XXXX`（或 `extract_code=XXXX`）
  - 详情：`GET /share/info/{code}`
- 静态资源
  - `GET /static/*`、`GET /favicon.ico`

提示：页面内已封装常用交互（上传、分享、复制链接等），一般使用浏览器即可体验全流程。

## 压测建议

- 简单压测（无 keep-alive）：
  - `webbench -c 1000 -t 5 http://127.0.0.1:8080/index.html`
- 更贴近真实场景（HTTP/1.1 keep-alive 与延迟分位）：推荐 wrk/hey/ab
  - 示例：`wrk -t4 -c200 -d30s http://127.0.0.1:8080/`
- 对大文件下载测试：使用 Range 请求与多并发下载分片，观察吞吐与 CPU 占用

## 常见问题

- 注册/登录失败：请确认已安装 MySQL 客户端开发包，并在 `Db` 相关实现中配置正确的数据库连接信息与表结构（默认连接参数在 `application/http_upload.cpp` 中构造 `Db` 时给出）。
- 上传目录：首次运行会自动创建 `uploads/`，文件名映射存于 `uploads/filename_mapping.json`。
- 连接在响应后关闭：静态页/资源当前默认 `Connection: close`，这是刻意设计，便于简单稳定；如需长连接可在 `StaticHandler`/`HttpResponse` 中调整。

## 许可证

暂未声明。若需开源协议，请在根目录补充 LICENSE 文件。
