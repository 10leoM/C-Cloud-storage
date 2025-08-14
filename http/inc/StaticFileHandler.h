#pragma once
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include "HttpRequest.h"
#include "HttpResponse.h"

// 简易静态文件与 Range 处理工具
class StaticFileHandler 
{
public:
    // 根目录 root (不包含最后的 /), request path (以 / 开头)
    static bool Handle(const std::string& root, const HttpRequest& req, HttpResponse* resp) 
    {
        std::string path = req.GetUrl(); // 已不含 query
        if (path == "/") path = "/index.html";
        std::string full = root + path; // 简单拼接，后续可加入 .. 校验
        struct stat st{};
        if (stat(full.c_str(), &st) < 0 || !S_ISREG(st.st_mode)) {
            *resp = HttpResponse::MakeSimple(true, HttpStatusCode::NotFound, "Not Found", "404 Not Found\n");
            return true; // 同步
        }
        int fd = ::open(full.c_str(), O_RDONLY);
        if (fd < 0) {
            *resp = HttpResponse::MakeSimple(true, HttpStatusCode::InternalServerError, "Internal Server Error", "open failed\n");
            return true;
        }
        resp->EnableAcceptRanges();
        resp->SetFileFd(fd);
        resp->SetBodyType(FILE_TYPE);
        resp->SetStatusCode(HttpStatusCode::OK);
        resp->SetStatusMessage("OK");
        resp->SetContentLength(static_cast<int>(st.st_size));
        // MIME 简化
        resp->SetContentType("application/octet-stream");
        // 解析 Range
        const_cast<HttpRequest&>(req).ParseRangeHeader();
        if (req.HasRange()) {
            long long start=0, end=0;
            if (req.IsRangeSuffix()) {
                long long suffix = req.GetRangeEnd();
                if (suffix <= 0) { *resp = HttpResponse::MakeSimple(true, HttpStatusCode::BadRequest, "Bad Request", "invalid suffix\n"); return true; }
                if (suffix > st.st_size) suffix = st.st_size;
                start = st.st_size - suffix;
                end = st.st_size - 1;
            } else {
                start = req.GetRangeStart();
                if (start >= st.st_size) {
                    // 416
                    *resp = HttpResponse::MakeSimple(true, HttpStatusCode::RangeNotSatisfiable, "Range Not Satisfiable", "416 Range Not Satisfiable\n");
                    resp->AddHeader("Content-Range", "bytes */" + std::to_string(st.st_size));
                    return true;
                }
                if (req.GetRangeEnd() >= 0) end = std::min<long long>(req.GetRangeEnd(), st.st_size - 1); else end = st.st_size - 1;
            }
            resp->SetContentRange(start, end, st.st_size);
        }
        return true;
    }
};
