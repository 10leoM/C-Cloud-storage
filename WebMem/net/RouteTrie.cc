// net/RouteTrie.cc
#include "RouteTrie.h"
#include <sstream>
#include "base/Logging.h"

namespace mymuduo {
namespace net {

std::vector<std::string> RouteTrie::splitPath(const std::string& path) {
    std::vector<std::string> segments;
    std::string segment;
    std::istringstream pathStream(path);
    
    while (std::getline(pathStream, segment, '/')) {
        if (!segment.empty()) {
            segments.push_back(segment);
        }
    }
    return segments;
}

void RouteTrie::addRoute(const std::string& path, const std::string& method, 
                        const std::string& handler, const std::vector<std::string>& paramNames) {
    auto current = root_;
    auto segments = splitPath(path);
    
    for (const auto& segment : segments) {
        if (segment[0] == ':') {
            // 参数节点
            current->paramNames.push_back(segment.substr(1));
            if (!current->children["*"]) {
                current->children["*"] = std::make_shared<TrieNode>();
            }
            current = current->children["*"];
        } else {
            // 静态节点
            if (!current->children[segment]) {
                current->children[segment] = std::make_shared<TrieNode>();
            }
            current = current->children[segment];
        }
    }
    
    current->isLeaf = true;
    current->handlers[method] = handler;
    LOG_INFO << "Added route: " << path << " with method: " << method;
}

RouteMatch RouteTrie::findRoute(const std::string& path, const std::string& method) {
    LOG_INFO << "Finding route for path: " << path << ", method: " << method;
    
    // 分离路径和查询参数
    size_t queryPos = path.find('?');
    std::string basePath = (queryPos != std::string::npos) ? path.substr(0, queryPos) : path;
    
    auto current = root_;
    std::unordered_map<std::string, std::string> params;
    auto segments = splitPath(basePath);
    
    // 存储所有可能的匹配结果
    std::vector<std::pair<std::shared_ptr<TrieNode>, std::unordered_map<std::string, std::string>>> matches;
    
    // 递归查找所有可能的匹配
    findMatches(current, segments, 0, params, matches);
    
    // 选择最佳匹配
    if (!matches.empty()) {
        // 优先选择静态路由匹配
        for (const auto& match : matches) {
            if (match.first->isLeaf && match.first->handlers.find(method) != match.first->handlers.end()) {
                LOG_INFO << "Found matching route with handler: " << match.first->handlers[method];
                return RouteMatch{match.first->handlers[method], match.second};
            }
        }
    }
    
    LOG_INFO << "No matching route found";
    return RouteMatch{"", {}};
}

void RouteTrie::findMatches(std::shared_ptr<TrieNode> node, 
                          const std::vector<std::string>& segments,
                          size_t currentIndex,
                          std::unordered_map<std::string, std::string> currentParams,
                          std::vector<std::pair<std::shared_ptr<TrieNode>, 
                          std::unordered_map<std::string, std::string>>>& matches) {
    if (currentIndex >= segments.size()) {
        if (node->isLeaf) {
            matches.push_back({node, currentParams});
        }
        return;
    }
    
    const std::string& currentSegment = segments[currentIndex];
    
    // 1. 尝试精确匹配
    if (node->children.find(currentSegment) != node->children.end()) {
        findMatches(node->children[currentSegment], segments, currentIndex + 1, 
                   currentParams, matches);
    }
    
    // 2. 尝试参数匹配
    if (node->children.find("*") != node->children.end()) {
        auto paramNode = node->children["*"];
        if (!paramNode->paramNames.empty()) {
            std::unordered_map<std::string, std::string> newParams = currentParams;
            newParams[paramNode->paramNames.back()] = currentSegment;
            findMatches(paramNode, segments, currentIndex + 1, newParams, matches);
        }
    }
    
    // 3. 尝试通配符匹配
    if (node->children.find("**") != node->children.end()) {
        auto wildcardNode = node->children["**"];
        // 通配符可以匹配任意数量的段
        for (size_t i = currentIndex; i <= segments.size(); ++i) {
            std::string wildcardValue;
            for (size_t j = currentIndex; j < i; ++j) {
                if (!wildcardValue.empty()) wildcardValue += "/";
                wildcardValue += segments[j];
            }
            if (!wildcardValue.empty()) {
                std::unordered_map<std::string, std::string> newParams = currentParams;
                newParams["*"] = wildcardValue;
                findMatches(wildcardNode, segments, i, newParams, matches);
            }
        }
    }
}

} // namespace net
} // namespace mymuduo