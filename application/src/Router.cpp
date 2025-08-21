#include "Router.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Connection.h"
#include <regex>
#include "StaticHandler.h"
#include "AuthHandler.h"
#include "FileHandler.h"
#include "ShareHandler.h"
#include "UserHandler.h"

void registerRoutes(
    Router &router,
    StaticHandler &staticHandler,
    AuthHandler &authHandler,
    FileHandler &fileHandler,
    ShareHandler &shareHandler,
    UserHandler &userHandler)
{
    // 公共路由（无需会话）
    router.addRouteExact("/favicon.ico", HttpMethod::kGet, [&staticHandler](auto &c, auto &r, auto *s)
                         { return staticHandler.handleFavicon(c, r, s); });
    router.addRouteExact("/register", HttpMethod::kPost, [&authHandler](auto &c, auto &r, auto *s)
                         { return authHandler.handleRegister(c, r, s); });
    router.addRouteExact("/login", HttpMethod::kPost, [&authHandler](auto &c, auto &r, auto *s)
                         { return authHandler.handleLogin(c, r, s); });
    router.addRouteExact("/", HttpMethod::kGet, [&staticHandler](auto &c, auto &r, auto *s)
                         { return staticHandler.handleIndex(c, r, s); });
    router.addRouteExact("/index.html", HttpMethod::kGet, [&staticHandler](auto &c, auto &r, auto *s)
                         { return staticHandler.handleIndex(c, r, s); });
    router.addRouteExact("/share.html", HttpMethod::kGet, [&staticHandler](auto &c, auto &r, auto *s)
                         { return staticHandler.handleIndex(c, r, s); });
    router.addRouteRegex("/static/(.*)", HttpMethod::kGet, [&staticHandler](auto &c, auto &r, auto *s)
                         { return staticHandler.handleStaticAsset(c, r, s); }, {"path"});
    router.addRouteExact("/register.html", HttpMethod::kGet, [&staticHandler](auto &c, auto &r, auto *s)
                         { return staticHandler.handleIndex(c, r, s); });
    // router.addRouteRegex("/share/([^/]+)", HttpMethod::kGet, [&shareHandler](auto &c, auto &r, auto *s)
    //                      { return shareHandler.handleShareAccess(c, r, s); }, {"code"});
    // router.addRouteRegex("/share/download/([^/]+)", HttpMethod::kGet, [&shareHandler](auto &c, auto &r, auto *s)
    //                      { return shareHandler.handleShareDownload(c, r, s); }, {"filename"});
    // router.addRouteRegex("/share/info/([^/]+)", HttpMethod::kGet, [&shareHandler](auto &c, auto &r, auto *s)
    //                      { return shareHandler.handleShareInfo(c, r, s); }, {"code"});

    // 需要会话验证的路由（具体校验放在 handler 内部）
    router.addRouteExact("/upload", HttpMethod::kPost, [&fileHandler](auto &c, auto &r, auto *s)
                         { return fileHandler.handleUpload(c, r, s); });
    router.addRouteExact("/files", HttpMethod::kGet, [&fileHandler](auto &c, auto &r, auto *s)
                         { return fileHandler.handleListFiles(c, r, s); });
    router.addRouteRegex("/download/([^/]+)", HttpMethod::kHead, [&fileHandler](auto &c, auto &r, auto *s)
                         { return fileHandler.handleDownload(c, r, s); }, {"filename"});
    router.addRouteRegex("/download/([^/]+)", HttpMethod::kGet, [&fileHandler](auto &c, auto &r, auto *s)
                         { return fileHandler.handleDownload(c, r, s); }, {"filename"});
    router.addRouteRegex("/delete/([^/]+)", HttpMethod::kDelete, [&fileHandler](auto &c, auto &r, auto *s)
                         { return fileHandler.handleDelete(c, r, s); }, {"filename"});
    // router.addRouteExact("/share", HttpMethod::kPost, [&shareHandler](auto &c, auto &r, auto *s)
    //                      { return shareHandler.handleShareFile(c, r, s); });
    // router.addRouteExact("/users/search", HttpMethod::kGet, [&userHandler](auto &c, auto &r, auto *s)
    //                      { return userHandler.handleSearchUsers(c, r, s); });
    router.addRouteExact("/logout", HttpMethod::kPost, [&authHandler](auto &c, auto &r, auto *s)
                         { return authHandler.handleLogout(c, r, s); });
}
