#include "inc/ShareHandler.h"
#include "inc/Util.h"
#include "inc/HttpUtil.h"
#include "inc/RangeUtil.h"
#include "inc/FileDownContext.h"
#include <regex>
#include <experimental/filesystem>
#include <nlohmann/json.hpp>

namespace fs = std::experimental::filesystem;
using json = nlohmann::json;

bool ShareHandler::handleShareFile(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp)
{
    std::string sessionId = req.GetHeader("X-Session-ID");
    int userId;
    std::string username;
    if (!auth_.validateSession(sessionId, userId, username))
    {
        sendError(resp, "未登录或会话已过期", HttpStatusCode::Unauthorized, conn);
        return true;
    }

    // 解析请求体
    try
    {
        json requestData = json::parse(req.GetBody());
        int fileId = requestData["fileId"];
        std::string shareType = requestData["shareType"]; // private|public|protected|user
        if (!sharesRepo_.isFileOwnedBy(fileId, userId))
        {
            sendError(resp, "您没有权限分享此文件", HttpStatusCode::Forbidden, conn);
            return true;
        }
        if (shareType == "private")
        {
            sharesRepo_.setPrivate(fileId);
            json out = {{"code", 0}, {"message", "文件设置为私有成功"}};
            sendJson(resp, out, conn);
            return true;
        }

        std::optional<int> expireHours;
        if (requestData.contains("expireTime") && !requestData["expireTime"].is_null())
        {
            int hours = requestData["expireTime"];
            if (hours > 0)
                expireHours = hours;
        }

        std::string shareCode = RandomString(32);
        std::optional<int> sharedWithId;        // 仅 shareType=='user' 时有效
        std::optional<std::string> extractCode; // 仅 shareType=='protected' 时有效
        if (shareType == "protected")
        {
            extractCode = RandomString(6);
        }
        else if (shareType == "user" && requestData.contains("sharedWithId"))
        {
            sharedWithId = requestData["sharedWithId"].get<int>();
            if (sharesRepo_.existsUserShare(fileId, *sharedWithId))
            {
                sendError(resp, "已经分享给该用户", HttpStatusCode::BadRequest, conn);
                return true;
            }
        }

        auto shareOpt = sharesRepo_.createShare(fileId, userId, sharedWithId, shareType, shareCode, extractCode, expireHours);
        if (!shareOpt)
        {
            sendError(resp, "分享失败", HttpStatusCode::InternalServerError, conn);
            return true;
        }
        int shareId = *shareOpt;
        json out = {{"code", 0}, {"message", "分享成功"}, {"shareId", shareId}, {"shareType", shareType}, {"shareCode", shareCode}, {"shareLink", "/share/" + shareCode}};
        if (shareType == "user" && sharedWithId)
            out["sharedWithId"] = *sharedWithId;
        else if (shareType == "protected" && extractCode)
            out["extractCode"] = *extractCode;
        sendJson(resp, out, conn);
        return true;
    }
    catch (const std::exception &e)
    {
        sendError(resp, "请求参数错误", HttpStatusCode::BadRequest, conn);
        return true;
    }
}

bool ShareHandler::handleShareAccess(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp)
{
    std::string path = req.GetUrl();
    std::smatch m;
    std::regex codeRegex("/share/([^/]+)");
    if (!std::regex_search(path, m, codeRegex) || m.size() < 2)
    {
        sendError(resp, "无效的分享链接", HttpStatusCode::BadRequest, conn);
        return true;
    }

    std::string shareCode = m[1];
    std::string accept = req.GetHeader("Accept"); // 判断是浏览器访问还是下载
    std::string sessionId = req.GetHeader("X-Session-ID");
    int userId = 0;
    std::string username;
    bool isAuthed = auth_.validateSession(sessionId, userId, username);
    // AJAX 或 Accept: application/json 视为 API 请求，返回 JSON
    if (req.GetHeader("X-Requested-With") == "XMLHttpRequest" || accept.find("application/json") != std::string::npos)
    {
        if (shareCode.empty() || shareCode.length() != 32)
        {
            sendError(resp, "无效的分享码格式", HttpStatusCode::BadRequest, conn);
            return true;
        }
        if (!std::all_of(shareCode.begin(), shareCode.end(), [](char c)
                         { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); }))
        {
            sendError(resp, "分享码包含非法字符", HttpStatusCode::BadRequest, conn);
            return true;
        }
        std::string extractCode = req.GetPathParam("code");
        auto recOpt = sharesRepo_.getByCode(shareCode);
        if (!recOpt)
        {
            sendError(resp, "分享链接已失效或不存在", HttpStatusCode::NotFound, conn);
            return true;
        }
        const auto &rec = *recOpt;
        bool isOwner = (rec.fileOwnerId == userId);
        bool permitted = false;
        if (isOwner)
            permitted = true;
        else if (rec.shareType == "public")
            permitted = true;
        else if (rec.shareType == "protected")
        {
            if (rec.extractCode && !extractCode.empty() && extractCode == *rec.extractCode)
                permitted = true;
        }
        else if (rec.shareType == "user")
        {
            if (isAuthed && rec.sharedWithId && userId == *rec.sharedWithId)
                permitted = true;
        }
        if (!permitted)
        {
            sendError(resp, (rec.shareType == "protected" ? "需要正确的提取码" : "您没有权限访问此文件"), HttpStatusCode::Forbidden, conn);
            return true;
        }
        json out = {{"code", 0}, {"message", "success"}, {"file", {{"id", rec.shareId}, {"fileId", rec.fileId}, {"ownerId", rec.ownerId}, {"sharedWithId", rec.sharedWithId.value_or(0)}, {"shareType", rec.shareType}, {"shareCode", rec.shareCode}, {"createdAt", rec.createdAt}, {"expireTime", rec.expireTime.value_or("")}, {"filename", rec.serverFilename}, {"originalName", rec.originalFilename}, {"size", rec.fileSize}, {"type", rec.fileType}, {"ownerUsername", rec.ownerUsername}, {"isOwner", isOwner}}}, {"downloadUrl", "/share/download/" + rec.serverFilename + "?code=" + shareCode}};
        sendJson(resp, out, conn);
        return true;
    }
    else    // 浏览器访问
    {
        // 浏览器访问时也需要验证分享码基本格式
        if (shareCode.empty() || shareCode.length() != 32)
        {
            sendError(resp, "无效的分享链接", HttpStatusCode::BadRequest, conn);
            return true;
        }
        if (!std::all_of(shareCode.begin(), shareCode.end(), [](char c)
                         { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); }))
        {
            sendError(resp, "分享码包含非法字符", HttpStatusCode::BadRequest, conn);
            return true;
        }
        
        // 验证分享码是否存在
        auto recOpt = sharesRepo_.getByCode(shareCode);
        if (!recOpt)
        {
            sendError(resp, "分享链接已失效或不存在", HttpStatusCode::NotFound, conn);
            return true;
        }
        
        // 分享码有效，返回前端页面
        bool result = staticHandler_.handleIndex(conn, req, resp);
        resp->AddHeader("X-Share-Code", shareCode);
        return result;
    }
}

bool ShareHandler::handleShareDownload(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp)
{
    std::string filename = req.GetPathParam("filename");
    if (filename.empty())
    {
        sendError(resp, "Missing filename", HttpStatusCode::BadRequest, conn);
        return true;
    }
    std::string shareCode = req.GetQueryValue("code");
    std::string extractCode = req.GetQueryValue("extract_code");
    if(shareCode.empty() || shareCode.length() != 32)
    {
        sendError(resp, "无效的分享码格式", HttpStatusCode::BadRequest, conn);
        return true;
    }

    std::string sessionId = req.GetHeader("X-Session-ID");
    int userId = 0;
    std::string username;
    bool isAuthed = auth_.validateSession(sessionId, userId, username);
    auto recOpt = sharesRepo_.getByCode(shareCode);
    if (!recOpt)
    {
        sendError(resp, "分享链接已失效或不存在", HttpStatusCode::NotFound, conn);
        return true;
    }
    const auto &rec = *recOpt;
    if (rec.serverFilename != filename)
    {
        sendError(resp, "文件不存在", HttpStatusCode::NotFound, conn);
        return true;
    }

    bool permitted = false;
    if(rec.shareType == "public")
        permitted = true;
    else if(rec.shareType == "protected")
    {
        if(extractCode.empty())
        {
            sendError(resp, "需要提取码", HttpStatusCode::Forbidden, conn);
            return true;
        }
        if(rec.extractCode && extractCode == *rec.extractCode)
            permitted = true;
    }
    else if(rec.shareType == "user")
    {
        if(isAuthed && rec.sharedWithId && userId == *rec.sharedWithId)
            permitted = true;
    }
    if(!permitted)
    {
        sendError(resp, (rec.shareType == "protected" ? "需要正确的提取码" : "您没有权限访问此文件"), HttpStatusCode::Forbidden, conn);
        return true;
    }

    // 构建分享文件下载的回复信息
    std::string filepath = uploadDir + "/" + rec.serverFilename;
    if (!fs::exists(filepath) || !fs::is_regular_file(filepath))
    {
        sendError(resp, "文件不存在", HttpStatusCode::NotFound, conn);
        return true;
    }
    uintmax_t fileSize = fs::file_size(filepath);
    if (req.GetMethod() == HttpMethod::kHead)
    {
        resp->SetStatusCode(HttpStatusCode::OK);
        resp->SetStatusMessage("OK");
        resp->SetContentType("application/octet-stream");
        resp->SetContentLength(fileSize);
        resp->AddHeader("Accept-Ranges", "bytes");
        resp->AddHeader("Connection", "close");
        conn->setWriteCompleteCallback([](const std::shared_ptr<Connection> &c){ c->shutdown(); return true; });
        return true;
    }
    // 处理 Range 请求
    auto rs = HttpRange::parse(req.GetHeader("Range"), fileSize);
    if (rs.isRange && !rs.satisfiable)
    {
        sendError(resp, "Range Not Satisfiable", HttpStatusCode::RangeNotSatisfiable, conn);
        return true;
    }
    auto httpContext = conn->GetContext();
    if (!httpContext)
    {
        sendError(resp, "Internal Server Error", HttpStatusCode::InternalServerError, conn);
        return true;
    }
    std::shared_ptr<FileDownContext> downContext = httpContext->GetContext<FileDownContext>();
    if (!downContext)
    {
        downContext = std::make_shared<FileDownContext>(filepath, rec.originalFilename);
        httpContext->SetContext(downContext);
        if (rs.isRange)
        {
            resp->SetStatusCode(HttpStatusCode::PartialContent);
            resp->SetStatusMessage("Partial Content");
            resp->AddHeader("Content-Range", "bytes " + std::to_string(rs.start) + "-" + std::to_string(rs.end) + "/" + std::to_string(fileSize));
        }
        else
        {
            resp->SetStatusCode(HttpStatusCode::OK);
            resp->SetStatusMessage("OK");
        }
        resp->SetContentType("application/octet-stream");
        resp->SetBodyType(HTML_TYPE);
        resp->SetContentLength(fileSize);
        resp->AddHeader("Content-Disposition", "attachment; filename=\"" + rec.originalFilename + "\"");
        resp->AddHeader("Accept-Ranges", "bytes");
        resp->AddHeader("Connection", "keep-alive");
        downContext->seekTo(rs.isRange ? rs.start : 0);
    }
    conn->setWriteCompleteCallback([downContext](const std::shared_ptr<Connection> &c)
                                   { std::string chunk; if (downContext->readNextChunk(chunk)) { c->Send(chunk); return true; } c->shutdown(); return true; });
    return true;
}

bool ShareHandler::handleShareInfo(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp)
{
    std::string shareCode = req.GetPathParam("code");
    if (shareCode.empty())
    {
        sendError(resp, "Missing share code", HttpStatusCode::BadRequest, conn);
        return true;
    }
    std::string extractCode = req.GetQueryValue("extract_code");
    auto recOpt = sharesRepo_.getByCode(shareCode);
    if (!recOpt)
    {
        sendError(resp, "分享链接已失效或不存在", HttpStatusCode::NotFound, conn);
        return true;
    }
    const auto &rec = *recOpt;
    if (rec.shareType == "protected")
    {
        if (!rec.extractCode || extractCode.empty() || extractCode != *rec.extractCode)
        {
            sendError(resp, "需要正确的提取码", HttpStatusCode::Forbidden, conn);
            return true;
        }
    }
    json out = {{"code", 0}, {"message", "success"}, {"shareType", rec.shareType}, {"file", {{"id", rec.shareId}, {"name", rec.serverFilename}, {"originalName", rec.originalFilename}, {"size", rec.fileSize}, {"type", rec.fileType}, {"shareTime", rec.createdAt}, {"expireTime", rec.expireTime.value_or("")}}}};
    sendJson(resp, out, conn);
    return true;
}