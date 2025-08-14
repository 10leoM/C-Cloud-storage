#pragma once

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// 路由匹配结果
struct RouteMatch
{
    std::string handler;                                 // 处理器标识/名字
    std::unordered_map<std::string, std::string> params; // 提取的路径参数
};

// Trie树节点
class TrieNode
{
public:
    std::map<std::string, std::shared_ptr<TrieNode>> children; // 段 -> 子节点
    std::map<std::string, std::string> handlers;               // method -> handler 名称
    std::vector<std::string> paramNames;                       // 该层路径的参数名栈（配合 "*" 参数节点）,如/:user/:id,则paramNames为["user","id"]
    bool isLeaf{false};
};

// Trie 路由
class RouteTrie
{
public:
    RouteTrie();

    // 添加路由：path 允许静态段、:param 参数段、"**" 通配段（可选）
    void addRoute(const std::string &path,
                  const std::string &method,
                  const std::string &handler,
                  const std::vector<std::string> &paramNames = {});

    // 查找路由：返回匹配的 handler 与参数；未命中返回 {"", {}}
    RouteMatch findRoute(const std::string &path, const std::string &method);

    void clear();

private:
    std::vector<std::string> splitPath(const std::string &path);

    void findMatches(std::shared_ptr<TrieNode> node,
                     const std::vector<std::string> &segments,
                     size_t currentIndex,
                     std::unordered_map<std::string, std::string> currentParams,
                     std::vector<std::pair<std::shared_ptr<TrieNode>,
                                           std::unordered_map<std::string, std::string>>> &matches);

    std::shared_ptr<TrieNode> root_;            // 根节点
};
