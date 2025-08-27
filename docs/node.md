
模块搭建顺序（文件/子系统层面）
CMake 基础
CMakeLists.txt：先建立 http_upload 目标与 include 目录，便于边写边编。

配置与基础设施
Config.h/.cc：端口、DB、上传目录、静态目录等集中配置（支持环境变量）。
Db.h/.cc：数据库封装；确保连接/逃逸/查询接口稳定。
Util.h/.cc：通用工具（URL 解码、随机串、MIME/后缀等）。
FilenameMap.h/.cc：文件名映射的持久化封装。
FileUploadContext.h/.cc、FileDownContext.h/.cc：上传/下载流式上下文。

数据访问层（Repository）
UsersRepository.h
FilesRepository.h
SharesRepository.h

业务 Handler
AuthHandler.h/.cc（依赖 Db/Config）
StaticHandler.h/.cc（依赖 Config/静态目录）
FileHandler.h/.cc（依赖 Db/Auth/FilenameMap/FilesRepository/SharesRepository/上传目录）
ShareHandler.h/.cc（依赖 Db/Auth/Static/SharesRepository/FilesRepository/上传目录）
UserHandler.h/.cc（依赖 Db/Auth/UsersRepository）

路由与装配
Router.h/.cc（已存在）
Routes.h/.cc：集中注册所有路由，直连各 Handler 方法。

应用入口
http_upload.cc：组装依赖、建立服务器、注册回调、启动事件循环。
验证节奏：每完成一块，先编译；Handlers 加完一类路由就跑一次最小“烟雾测试”（GET /、/files、/upload、/share）。
http_upload.cc 的编写顺序（文件内部）
头部引用：基础 net/base、Config、Db、Util、Handlers、Routes、HttpUtil。
成员定义：uploadDir_/mappingFile_/FilenameMap/Db/Handlers/Router 等。
构造与初始化：
读取 Config，设置上传/静态目录，确保目录存在；
连接数据库；
加载 FilenameMap；
构造各 Handler（把 Db/Auth/FilenameMap/上传目录/Config 注入）；
调用 registerRoutes(router_, static_, auth_, file_, share_, user_)。
连接与请求回调：
onConnection：创建/清理 HttpContext；
onRequest：日志、交给 router_.dispatch，统一 404/500。
析构：保存 FilenameMap、让 Db 析构自动断开。

main：
设日志级别、建 EventLoop/HttpServer；
new HttpUploadHandler 并设置连接/HTTP 回调；
server.start()、loop.loop()

小提示
确保 CMakeLists.txt 已包含 Routes.cc 和你新增的 Config.cc。
Handler 内部做鉴权校验；Routes 只做路由分发。
任何一步都保持“构建可用 + 最小可运行路径”，便于快速定位问题。