#include "inc/FileHandler.h"
#include "Connection.h"
#include "HttpUtil.h"
#include "Util.h"
#include "FileDownContext.h"
#include "FileUploadContext.h"
#include "FileRepository.h"
#include "Logger.h"
#include "RangeUtil.h"
#include <nlohmann/json.hpp>
#include <experimental/filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>

using json = nlohmann::json;
namespace fs = std::experimental::filesystem;

FileHandler::FileHandler(Db &db, AuthHandler &auth, FilenameMap &fmap, const std::string &uploadDir)
    : db_(db), auth_(auth), fmap_(fmap), uploadDir_(uploadDir), filesRepo_(db), sharesRepo_(db) {}

bool FileHandler::handleListFiles(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp)
{
    std::string sessionId = req.GetHeader("X-Session-ID");
    int userId;
    std::string username;
    if (!auth_.validateSession(sessionId, userId, username))
    {
        sendError(resp, "未登录或会话已过期", HttpStatusCode::Unauthorized, conn);
        return true;
    }

    std::string listType = req.GetQueryValue("type");
    listType = listType == "" ? "my" : listType;
    json response;
    response["code"] = 0;
    response["message"] = "Success";
    json files = json::array();
    std::vector<FileRow> rows;
    if (listType == "my")
        rows = filesRepo_.listMyFiles(userId);
    else if (listType == "shared")
        rows = filesRepo_.listSharedFiles(userId);
    else if (listType == "all")
        rows = filesRepo_.listAllFiles(userId);

    for (const auto &fr : rows)
    {
        json shareInfo = nullptr;
        if (fr.isOwner)
        {
            auto si = filesRepo_.getShareInfo(fr.id);
            if (si)
            {
                shareInfo = {{"type", si->shareType}};
                if (si->shareCode)
                    shareInfo["shareCode"] = *si->shareCode;
                if (si->extractCode && si->shareType == "protected")
                    shareInfo["extractCode"] = *si->extractCode;
                if (si->shareWithId && si->shareType == "user")
                {
                    shareInfo["sharedWithId"] = *si->shareWithId;
                    if (si->sharedWithUsername)
                        shareInfo["sharedWithUsername"] = *si->sharedWithUsername;
                }
                if (si->expireTime)
                    shareInfo["expireTime"] = *si->expireTime;
            }
        }
        json fileInfo = {{"id", fr.id}, {"name", fr.filename}, {"originalName", fr.originalFilename}, {"size", fr.size}, {"type", fr.type}, {"createdAt", fr.createdAt}, {"isOwner", fr.isOwner}};
        if (shareInfo)
            fileInfo["shareInfo"] = shareInfo;
        files.push_back(fileInfo);
    }
    response["files"] = files;
    sendJson(resp, response, conn, HttpStatusCode::OK);
    return true;
}

bool FileHandler::handleDelete(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp)
{
    std::string sessionId = req.GetHeader("X-Session-ID");
    int userId;
    std::string username;
    if (!auth_.validateSession(sessionId, userId, username))
    {
        sendError(resp, "未登录或会话已过期", HttpStatusCode::Unauthorized, conn);
        return true;
    }
    // 获取文件名
    std::string filename = req.GetPathParam("filename");
    if (filename.empty())
    {
        sendError(resp, "缺少文件名", HttpStatusCode::BadRequest, conn);
        return true;
    }
    // 获取文件ID
    std::string filepath = uploadDir_ + "/" + filename;
    auto fileIdOpt = filesRepo_.getOwnedFileIdByServerFilename(filename, userId);
    if (!fileIdOpt)
    {
        sendError(resp, "文件不存在或无权限删除", HttpStatusCode::NotFound, conn);
        return true;
    }
    int fileId = *fileIdOpt;

    // 删除文件记录
    if (!filesRepo_.deleteFileById(fileId))
    {
        sendError(resp, "删除文件失败", HttpStatusCode::InternalServerError, conn);
        return true;
    }

    // 删除文件
    if (access(filepath.c_str(), F_OK) == 0)
    {
        if (unlink(filepath.c_str()) != 0)
        {
            LOG_WARN << "Failed to delete file: " << filepath;
        }
    }
    fmap_.erase(filename);
    json out = {{"code", 0}, {"message", "success"}};
    sendJson(resp, out, conn, HttpStatusCode::OK);
    return true;
}

bool FileHandler::handleDownload(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp)
{
    std::string sessionId = req.GetHeader("X-Session-ID");
    if (sessionId.empty())
        sessionId = req.GetQueryValue("sessionId");
    int userId = 0;
    std::string username;
    bool isAuthenticated = auth_.validateSession(sessionId, userId, username);

    // 获取文件名
    std::string filename = req.GetPathParam("filename");
    if (filename.empty())
    {
        sendError(resp, "丢失文件", HttpStatusCode::BadRequest, conn);
        return true;
    }

    std::string shareCode = req.GetQueryValue("code");           // 分享码
    std::string extractCode = req.GetQueryValue("extract_code"); // 提取码,私有时使用
    std::string serverFilename;
    std::string originalFilename;
    int ownerId = 0;
    bool permitted = false;

    if (!shareCode.empty()) // 分享下载
    {
        auto shareOpt = sharesRepo_.getByCode(shareCode);
        if (!shareOpt)
        {
            sendError(resp, "无效的分享码或分享已过期", HttpStatusCode::NotFound, conn);
            return true;
        }
        ShareRecord share = *shareOpt;
        serverFilename = share.serverFilename;
        originalFilename = share.originalFilename;
        ownerId = share.fileOwnerId;
        if (serverFilename != filename)
        {
            sendError(resp, "文件不存在", HttpStatusCode::NotFound, conn);
            return true;
        }

        if (isAuthenticated && userId == ownerId) // 文件所有者是自己
            permitted = true;
        else if (share.shareType == "public") // 公开分享，任何人都可以下载
            permitted = true;
        else if (share.shareType == "protected" && share.extractCode && !extractCode.empty() && extractCode == *share.extractCode) // 受保护分享，需要提取码
            permitted = true;
        else if (share.shareType == "user" && isAuthenticated && share.sharedWithId && userId == *share.sharedWithId) // 私有分享，需要分享给的用户是当前用户
            permitted = true;
    }
    else // 无分享码，下载自己的文件
    {
        if (!isAuthenticated)
        {
            sendError(resp, "请先登录", HttpStatusCode::Unauthorized, conn);
            return true;
        }
        auto fileOpt = filesRepo_.getByServerFilename(filename);
        if (!fileOpt)
        {
            sendError(resp, "文件不存在", HttpStatusCode::NotFound, conn);
            return true;
        }
        serverFilename = fileOpt->serverFilename;
        originalFilename = fileOpt->originalFilename;
        ownerId = fileOpt->ownerId;
        permitted = (userId == ownerId);
    }

    // 处理下载

    if (!permitted) // 无权限下载
    {
        sendError(resp, "无权限下载", HttpStatusCode::Forbidden, conn);
        return true;
    }

    std::string filepath = uploadDir_ + "/" + serverFilename;
    if (!fs::exists(filepath) || !fs::is_regular_file(filepath))
    {
        sendError(resp, "文件不存在", HttpStatusCode::NotFound, conn);
        return true;
    }
    uintmax_t fileSize = fs::file_size(filepath);

    // 处理 HEAD 请求
    if (req.GetMethod() == HttpMethod::kHead)
    {
        resp->SetStatusCode(HttpStatusCode::OK);
        resp->SetStatusMessage("OK");
        resp->SetContentType("application/octet-stream");
        resp->AddHeader("Content-Length", std::to_string(fileSize));
        resp->AddHeader("Accept-Ranges", "bytes");
        resp->AddHeader("Connection", "close");
        conn->setWriteCompleteCallback([](const std::shared_ptr<Connection> &c)
                                       { c->shutdown(); return true; });
        return true;
    }

    // 解析 Range 头
    std::string rangeHeader = req.GetHeader("Range");
    HttpRange::RangeSpec rs = HttpRange::parse(rangeHeader, fileSize);
    if (rs.isRange && !rs.satisfiable) // 无法满足的 Range 请求
    {
        sendError(resp, "无法满足的 Range 请求", HttpStatusCode::RangeNotSatisfiable, conn);
        return true;
    }

    std::shared_ptr<HttpContext> httpContext = conn->getContext<HttpContext>();
    if (!httpContext)
    {
        sendError(resp, "Internal Server Error", HttpStatusCode::InternalServerError, conn);
        return true;
    }
    std::shared_ptr<FileDownContext> downContext = conn->getContext<FileDownContext>(); // 试图获取已有的下载上下文
    if (!downContext)
    {
        downContext = std::make_shared<FileDownContext>(filepath, originalFilename);
        conn->setContext(downContext);
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
        resp->AddHeader("Content-Disposition", "attachment; filename=\"" + originalFilename + "\"");
        resp->AddHeader("Accept-Ranges", "bytes");
        resp->AddHeader("Connection", "keep-alive");
        downContext->seekTo(rs.isRange ? rs.start : 0);
    }
    conn->setWriteCompleteCallback([downContext](const std::shared_ptr<Connection> &c)
                                   {
        std::string chunk; 
        if (downContext->readNextChunk(chunk)) { c->Send(chunk); return true; }
        c->shutdown(); 
        return true; });

    return true;
}

bool FileHandler::handleUpload(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp)
{
    // 验证用户身份
    std::string sessionId = req.GetHeader("X-Session-ID");
    int userId;
    std::string username;
    if (!auth_.validateSession(sessionId, userId, username))
    {
        sendError(resp, "未登录或会话已过期", HttpStatusCode::Unauthorized, conn);
        return true;
    }

    // 处理上传
    auto httpContext = std::static_pointer_cast<HttpContext>(conn->getContext<HttpContext>());
    if (!httpContext)
    {
        sendError(resp, "Internal Server Error", HttpStatusCode::InternalServerError, conn);
        return true;
    }
    std::shared_ptr<FileUploadContext> uploadContext = conn->getContext<FileUploadContext>();
    // 初始化上传上下文
    if (!uploadContext)
    {
        std::string contentType = req.GetHeader("Content-Type");
        if (contentType.empty())
        {
            sendError(resp, "Content-Type header is missing", HttpStatusCode::BadRequest, conn);
            return true;
        }
        std::smatch m;
        std::regex boundaryRegex("boundary=(.+)$");
        if (!std::regex_search(contentType, m, boundaryRegex))
        {
            sendError(resp, "Invalid Content-Type", HttpStatusCode::BadRequest, conn);
            return true;
        }
        std::string boundary = "--" + m[1].str();
        std::string originalFilename;
        std::string headerFilename = req.GetHeader("X-File-Name");
        if (!headerFilename.empty())
            originalFilename = UrlDecode(headerFilename);
        else
        {
            std::string body = req.GetBody();
            std::regex filenameRegex("Content-Disposition:.*filename=\"([^\"]+)\"");
            if (std::regex_search(body, m, filenameRegex) && m[1].matched)
                originalFilename = m[1].str();
            else
                originalFilename = "unknown_file";
        }
        std::string filename = UniqueFilename("upload");
        std::string filepath = uploadDir_ + "/" + filename;
        uploadContext = std::make_shared<FileUploadContext>(filepath, originalFilename);
        conn->setContext(uploadContext);
        uploadContext->setBoundary(boundary);
        std::string body = req.GetBody();
        size_t pos = body.find("\r\n\r\n");
        if (pos != std::string::npos)
        {
            pos += 4;
            std::string endBoundary = boundary + "--";
            size_t endPos = body.find(endBoundary);
            if (endPos != std::string::npos)
            {
                if (endPos > 0)
                    uploadContext->writeData(body.data() + pos, endPos - pos);
                uploadContext->setState(FileUploadContext::State::kComplete);
            }
            else
            {
                if (pos < body.size())
                    uploadContext->writeData(body.data() + pos, body.size() - pos);
                uploadContext->setState(FileUploadContext::State::kExpectBoundary);
            }
        }
        req.SetBody("");
    }
    else   // 已有上传上下文，继续处理
    {
        std::string body = req.GetBody();
        if (!body.empty())
        {
            switch (uploadContext->getState())
            {
            case FileUploadContext::State::kExpectBoundary:
            {
                std::string endBoundary = uploadContext->getBoundary() + "--";
                size_t endPos = body.find(endBoundary);
                if (endPos != std::string::npos)
                {
                    if (endPos > 0)
                        uploadContext->writeData(body.data(), endPos);
                    uploadContext->setState(FileUploadContext::State::kComplete);
                    break;
                }
                size_t boundaryPos = body.find(uploadContext->getBoundary());
                if (boundaryPos != std::string::npos)
                {
                    size_t contentStart = body.find("\r\n\r\n", boundaryPos);
                    if (contentStart != std::string::npos)
                    {
                        contentStart += 4;
                        if (boundaryPos > 0)
                            uploadContext->writeData(body.data(), boundaryPos);
                        uploadContext->setState(FileUploadContext::State::kExpectContent);
                    }
                }
                else
                {
                    uploadContext->writeData(body.data(), body.size());
                }
                break;
            }
            case FileUploadContext::State::kExpectContent:
            {
                size_t boundaryPos = body.find(uploadContext->getBoundary());
                if (boundaryPos != std::string::npos)
                {
                    uploadContext->writeData(body.data(), boundaryPos);
                    uploadContext->setState(FileUploadContext::State::kExpectBoundary);
                }
                else
                {
                    uploadContext->writeData(body.data(), body.size());
                }
                break;
            }
            case FileUploadContext::State::kComplete:
                break;
            default:
                break;
            }
        }
        req.SetBody("");
    }

    if (uploadContext->getState() == FileUploadContext::State::kComplete || httpContext->GetCompleteRequest())
    {
        std::string serverFilename = fs::path(uploadContext->getFilename()).filename().string();
        std::string originalFilename = uploadContext->getOriginalFilename();
        uintmax_t fileSize = uploadContext->getTotalBytes();
        std::string fileType = FileTypeByExt(originalFilename);
        auto fileIdOpt = filesRepo_.createFile(serverFilename, originalFilename, fileSize, fileType, userId);
        int fileId = fileIdOpt.value_or(0);
        json out = {{"code", 0}, {"message", "上传成功"}, {"fileId", fileId}, {"filename", serverFilename}, {"originalFilename", originalFilename}, {"size", fileSize}};
        sendJson(resp, out, conn);
        conn->setContext(nullptr);
        return true;
    }
    return false;
}