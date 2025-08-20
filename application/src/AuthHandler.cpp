#include "inc/AuthHandler.h"
#include "inc/HttpUtil.h"
#include "Logger.h"

bool AuthHandler::handleRegister(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp)
{
    try
    {
        json requestData = json::parse(req.GetBody());
        std::string username = requestData["username"];
        std::string password = requestData["password"];
        std::string email = requestData.value("email", "");
        if (username.empty() || password.empty())
        {
            sendError(resp, "用户名和密码不能为空", BadRequest, conn);
            return true;
        }
        std::string hashed = PseudoSha256(password);
        std::string check = "SELECT id FROM users WHERE username = '" + db_.escape(username) + "'";
        MYSQL_RES *r = db_.query(check);
        if (r && mysql_num_rows(r) > 0)
        {
            mysql_free_result(r);
            sendError(resp, "用户名已存在", BadRequest, conn);
            return true;
        }

        if (r)
            mysql_free_result(r);
        std::string ins = "INSERT INTO users (username, password, email) VALUES ('" + db_.escape(username) + "', '" + db_.escape(hashed) + "', " + (email.empty() ? "NULL" : "'" + db_.escape(email) + "'") + ")";
        if (!db_.exec(ins))
        {
            sendError(resp, "注册失败，请稍后重试", InternalServerError, conn);
            return true;
        }
        int userId = static_cast<int>(db_.insertId());
        json out = {{"code", 0}, {"message", "注册成功"}, {"userId", userId}};
        sendJson(resp, out, conn);
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "注册异常: " << e.what();
        sendError(resp, std::string("注册失败: ") + e.what(), InternalServerError, conn);
        return true;
    }
}

bool AuthHandler::handleLogin(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp)
{
    try
    {
        json requestData = json::parse(req.GetBody());
        std::string username = requestData["username"];
        std::string password = requestData["password"];
        if (username.empty() || password.empty())
        {
            sendError(resp, "用户名和密码不能为空", BadRequest, conn);
            return true;
        }
        std::string hashed = PseudoSha256(password);
        std::string q = "SELECT id, username FROM users WHERE username = '" + db_.escape(username) + "' AND password = '" + db_.escape(hashed) + "'";
        MYSQL_RES *r = db_.query(q);
        if (!r || mysql_num_rows(r) == 0)
        {
            if (r)
                mysql_free_result(r);
            sendError(resp, "用户名或密码错误", HttpStatusCode::Unauthorized, conn);
            return true;
        }
        MYSQL_ROW row = mysql_fetch_row(r);
        int userId = std::stoi(row[0]);
        std::string name = row[1];
        std::string sessionId = generateSessionId();
        saveSession(sessionId, userId, name);
        json out = {{"code", 0}, {"message", "登录成功"}, {"sessionId", sessionId}, {"userId", userId}, {"username", name}};
        sendJson(resp, out, conn);
        mysql_free_result(r);
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "登录异常: " << e.what();
        sendError(resp, std::string("登录失败: ") + e.what(), InternalServerError, conn);
        return true;
    }
}

bool AuthHandler::handleLogout(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp)
{
    std::string sessionId = req.GetHeader("Session-Id");
    if (sessionId.empty())
    {
        sendError(resp, "未提供会话ID", BadRequest, conn);
        return true;
    }
    endSession(sessionId);
    json out = {{"code", 0}, {"message", "登出成功"}};
    sendJson(resp, out, conn, HttpStatusCode::OK);
    return true;
}

bool AuthHandler::validateSession(const std::string &sessionId, int &userId, std::string &username)
{
    if (sessionId.empty())
        return false;
    std::string q = "SELECT user_id, username FROM sessions WHERE session_id = '" + db_.escape(sessionId) + "' AND expire_time > NOW()";
    MYSQL_RES *r = db_.query(q);
    if (!r || mysql_num_rows(r) == 0)
    {
        if (r)
            mysql_free_result(r);
        return false;
    }
    MYSQL_ROW row = mysql_fetch_row(r);
    userId = std::stoi(row[0]);
    username = row[1];
    mysql_free_result(r);
    return true;
}

void AuthHandler::endSession(const std::string &sessionId)
{
    if (sessionId.empty())
        return;
    std::string q = "DELETE FROM sessions WHERE session_id = '" + db_.escape(sessionId) + "'";
    db_.exec(q);
}

std::string AuthHandler::generateSessionId()
{
    return RandomString(32); // 生成一个随机的32字符长的会话ID
}

void AuthHandler::saveSession(const std::string &sessionId, int userId, const std::string &username)
{
    std::string query = "INSERT INTO sessions (session_id, user_id, username) VALUES ('" + db_.escape(sessionId) + "', " + std::to_string(userId) + ", '" + db_.escape(username) + "')";
    db_.exec(query);
}

