#include "inc/UserHandler.h"
#include "inc/HttpUtil.h"
#include "Logger.h"
#include <nlohmann/json.hpp>
#include "Util.h"

using json = nlohmann::json;

bool UserHandler::handleSearchUsers(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp)
{
    std::string sessionId = req.GetHeader("X-Session-ID");
    int userId;
    std::string username;
    if (!auth_.validateSession(sessionId, userId, username))
    {
        sendError(resp, "未登录或会话已过期", HttpStatusCode::Unauthorized, conn);
        LOG_WARN << "validateSession failed";
        return true;
    }

    std::string keyword = req.GetPathParam("keyword");
    if (keyword.empty())
    {
        sendError(resp, "搜索关键词不能为空", HttpStatusCode::BadRequest, conn);
        return true;
    }

    auto result = usersRepo_.searchByUsernameKeywordExcluding(keyword, userId, 10);
    json out;
    out["code"] = 0;
    out["message"] = "Success";
    json users = json::array();
    for (const auto &u : result)
    {
        users.push_back({{"id", u.id}, {"username", u.username}, {"email", u.email}});
    }
    out["users"] = users;

    sendJson(resp, out, conn);
    return true;
}
