#include "RouterTrie.h"
#include <sstream>


RouteTrie::RouteTrie() : root_(std::make_shared<TrieNode>()) {}

void RouteTrie::clear() { root_ = std::make_shared<TrieNode>(); }

std::vector<std::string> RouteTrie::splitPath(const std::string &path) 
{
    std::vector<std::string> segments;
    std::string segment;
    std::istringstream pathStream(path);
    while(std::getline(pathStream, segment, '/')) 
    {
        if(!segment.empty())
        segments.emplace_back(segment);
    }
    return segments;
}

void RouteTrie::addRoute(const std::string &path,
                         const std::string &method,
                         const std::string &handler,
                         const std::vector<std::string> &paramNames) 
{
    auto current = root_;
    std::vector<std::string> segments = splitPath(path);

    for (const auto &segment : segments) {
        if (!segment.empty() && segment[0] == ':') {
            // 参数节点使用 "*" 作为统一键（参数名应记录在子节点上）
            if (!current->children["*"]) {
                current->children["*"] = std::make_shared<TrieNode>();
            }
            current = current->children["*"];
            current->paramNames.push_back(segment.substr(1));
        } else if (segment == "**") {
            // 通配符节点（可选），使用 "**" 作为键
            if (!current->children["**"]) {
                current->children["**"] = std::make_shared<TrieNode>();
            }
            current = current->children["**"];
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
}

RouteMatch RouteTrie::findRoute(const std::string &path, const std::string &method) {
    // 去除查询串
    size_t pos = path.find('?');
    std::string basePath = (pos != std::string::npos) ? path.substr(0, pos) : path;

    auto current = root_;
    std::unordered_map<std::string, std::string> params;
    std::vector<std::string> segments = splitPath(basePath);

    std::vector<std::pair<std::shared_ptr<TrieNode>, std::unordered_map<std::string, std::string>>> matches;
    findMatches(current, segments, 0, params, matches);

    if (!matches.empty()) {
        for (const auto &m : matches) {
            if (m.first->isLeaf) {
                auto it = m.first->handlers.find(method);
                if (it != m.first->handlers.end()) {
                    return RouteMatch{it->second, m.second};
                }
            }
        }
    }
    return RouteMatch{"", {}};
}

void RouteTrie::findMatches(std::shared_ptr<TrieNode> node,
                            const std::vector<std::string> &segments,
                            size_t currentIndex,
                            std::unordered_map<std::string, std::string> currentParams,
                            std::vector<std::pair<std::shared_ptr<TrieNode>, std::unordered_map<std::string, std::string>>> &matches) {
    if (currentIndex >= segments.size()) {
        if (node->isLeaf) {
            matches.push_back({node, currentParams});
        }
        return;
    }

    const std::string &cur = segments[currentIndex];

    // 1) 静态匹配
    auto it = node->children.find(cur);
    if (it != node->children.end()) {
        findMatches(it->second, segments, currentIndex + 1, currentParams, matches);
    }

    // 2) 参数匹配（*）
    auto itParam = node->children.find("*");
    if (itParam != node->children.end()) {
        auto paramNode = itParam->second;
        if (!paramNode->paramNames.empty()) {
            auto nextParams = currentParams;
            nextParams[paramNode->paramNames.back()] = cur;
            findMatches(paramNode, segments, currentIndex + 1, nextParams, matches);
        }
    }

    // 3) 通配符匹配（**）
    auto itStarStar = node->children.find("**");
    if (itStarStar != node->children.end()) {
        auto wcNode = itStarStar->second;
        // ** 可以匹配 0..N 段
        for (size_t i = currentIndex; i <= segments.size(); ++i) {
            std::string captured;
            for (size_t j = currentIndex; j < i; ++j) {
                if (!captured.empty()) captured.push_back('/');
                captured.append(segments[j]);
            }
            auto nextParams = currentParams;
            if (!captured.empty()) nextParams["*"] = captured; // 统一命名为 "*"
            findMatches(wcNode, segments, i, nextParams, matches);
        }
    }
}
