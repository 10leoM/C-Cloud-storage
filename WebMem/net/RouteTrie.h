// net/RouteTrie.h
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <unordered_map>
#include "base/copyable.h"

namespace mymuduo {
namespace net {

// 路由匹配结果
struct RouteMatch {
    std::string handler;
    std::unordered_map<std::string, std::string> params;
};

// Trie树节点
class TrieNode {
public:
    std::map<std::string, std::shared_ptr<TrieNode>> children;
    std::map<std::string, std::string> handlers;  // method -> handler
    std::vector<std::string> paramNames;
    bool isLeaf;
    
    TrieNode() : isLeaf(false) {}
};

// Trie树路由实现
class RouteTrie : public mymuduo::copyable {
public:
    RouteTrie() : root_(std::make_shared<TrieNode>()) {}
    
    // 添加路由
    void addRoute(const std::string& path, const std::string& method, 
                 const std::string& handler, const std::vector<std::string>& paramNames = {});
    
    // 查找路由
    RouteMatch findRoute(const std::string& path, const std::string& method);
    
    // 清空路由
    void clear() { root_ = std::make_shared<TrieNode>(); }

private:
    // 分割路径
    std::vector<std::string> splitPath(const std::string& path);
    
    std::shared_ptr<TrieNode> root_;
    
    void findMatches(std::shared_ptr<TrieNode> node,
                    const std::vector<std::string>& segments,
                    size_t currentIndex,
                    std::unordered_map<std::string, std::string> currentParams,
                    std::vector<std::pair<std::shared_ptr<TrieNode>,
                    std::unordered_map<std::string, std::string>>>& matches);
};

} // namespace net
} // namespace mymuduo