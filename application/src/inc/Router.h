#pragma once
#include <regex>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Connection.h"
#include "StaticFileHandler.h"
#include "StaticHandler.h"
#include "AuthHandler.h"
#include "FileHandler.h"
#include "ShareHandler.h"
#include "UserHandler.h"

class Router
{
public:
    using Handler = std::function<bool(const std::shared_ptr<Connection> &, HttpRequest &, HttpResponse *)>;

    void addRouteExact(const std::string &path,
                       HttpMethod method,
                       Handler handler)
    {
        std::string pattern = "^" + escapeRegex(path) + "$";
        routes_.emplace_back(Route{std::regex(pattern), {}, std::move(handler), method});
    }

    void addRouteRegex(const std::string &pattern,
                       HttpMethod method,
                       Handler handler,
                       const std::vector<std::string> &params)
    {
        routes_.emplace_back(Route{std::regex(pattern), params, std::move(handler), method});
    }

    // 返回 true 表示找到并处理了路由
    bool dispatch(const std::shared_ptr<Connection> &conn, HttpRequest &req, HttpResponse *resp) const
    {
        const std::string &path = req.GetUrl();
        for (const auto &route : routes_)
        {
            if (route.method != req.GetMethod())
                continue;
            std::smatch matches;
            if (std::regex_match(path, matches, route.pattern))
            {
                // 提取路径参数
                std::map<std::string, std::string> params;
                for (size_t i = 0; i < route.params.size() && i + 1 < matches.size(); ++i)
                {
                    params[route.params[i]] = matches[i + 1];
                }
                req.SetPathParam(params);
                return route.handler(conn, req, resp);
            }
        }
        return false;
    }

private:
    struct Route
    {
        std::regex pattern;
        std::vector<std::string> params;
        Handler handler;
        HttpMethod method;
    };

    static std::string escapeRegex(const std::string &str)
    {
        std::string result;
        result.reserve(str.size() * 2);
        for (char c : str)
        {
            switch (c)
            {
            case '.':
            case '+':
            case '*':
            case '?':
            case '^':
            case '$':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case '|':
            case '\\':
                result += '\\';
                [[fallthrough]];
            default:
                result += c;
            }
        }
        return result;
    }

    std::vector<Route> routes_;
};

void registerRoutes(Router &router, StaticHandler &staticHandler, AuthHandler &authHandler, FileHandler &fileHandler, ShareHandler &shareHandler, UserHandler &userHandler);