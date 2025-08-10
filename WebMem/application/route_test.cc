#include <iostream>
#include <vector>
#include <regex>
#include <string>
#include <chrono>
#include <random>
#include <memory>
#include <map>
#include <unordered_map>
#include <functional>

// 路由模式结构（当前实现方式）
struct RoutePattern {
    std::regex pattern;
    std::vector<std::string> params;
    std::string handler;
    std::string method;

    RoutePattern(const std::string& pattern_str, 
                const std::vector<std::string>& param_names,
                const std::string& h,
                const std::string& m)
        : pattern(pattern_str)
        , params(param_names)
        , handler(h)
        , method(m)
    {}
};

// Trie树实现
class RouteTrie {
private:
    struct TrieNode {
        std::map<std::string, std::shared_ptr<TrieNode>> children;
        std::map<std::string, std::string> handlers;  // method -> handler
        std::vector<std::string> paramNames;
        bool isLeaf;
        
        TrieNode() : isLeaf(false) {}
    };
    
    std::shared_ptr<TrieNode> root;
    
    std::vector<std::string> splitPath(const std::string& path) {
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
    
public:
    RouteTrie() : root(std::make_shared<TrieNode>()) {}
    
    void addRoute(const std::string& path, const std::string& method, 
                 const std::string& handler, const std::vector<std::string>& paramNames) {
        auto current = root;
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
    }
    
    struct RouteMatch {
        std::string handler;
        std::unordered_map<std::string, std::string> params;
    };
    
    RouteMatch findRoute(const std::string& path, const std::string& method) {
        auto current = root;
        std::unordered_map<std::string, std::string> params;
        auto segments = splitPath(path);
        
        for (const auto& segment : segments) {
            if (current->children.find(segment) != current->children.end()) {
                current = current->children[segment];
            } else if (current->children.find("*") != current->children.end()) {
                current = current->children["*"];
                if (!current->paramNames.empty()) {
                    params[current->paramNames.back()] = segment;
                }
            } else {
                return RouteMatch{"", {}};
            }
        }
        
        if (current->isLeaf && 
            current->handlers.find(method) != current->handlers.end()) {
            return RouteMatch{current->handlers[method], params};
        }
        
        return RouteMatch{"", {}};
    }
};

// 性能测试类
class RouteBenchmark {
private:
    std::vector<RoutePattern> regexRoutes_;
    RouteTrie trieRoutes_;
    
    // 生成随机路径
    std::string generateRandomPath(int depth) {
        static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);
        
        std::string path;
        for (int i = 0; i < depth; ++i) {
            if (i > 0) path += '/';
            if (dis(gen) % 3 == 0) {  // 30%的概率生成参数路径
                path += ":param" + std::to_string(i);
            } else {
                int len = dis(gen) % 5 + 3;  // 3-7个字符
                for (int j = 0; j < len; ++j) {
                    path += chars[dis(gen)];
                }
            }
        }
        return path;
    }
    
    // 生成测试数据
    void generateTestData(int numRoutes) {
        std::vector<std::string> methods = {"GET", "POST", "PUT", "DELETE"};
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> methodDis(0, static_cast<int>(methods.size() - 1));
        std::uniform_int_distribution<> depthDis(1, 5);
        
        for (int i = 0; i < numRoutes; ++i) {
            std::string method = methods[methodDis(gen)];
            int depth = depthDis(gen);
            std::string path = generateRandomPath(depth);
            std::string handler = "handler" + std::to_string(i);
            
            // 添加正则路由
            std::string pattern = "^" + path + "$";
            std::vector<std::string> params;
            size_t pos = 0;
            while ((pos = path.find(":param", pos)) != std::string::npos) {
                size_t end = path.find('/', pos);
                if (end == std::string::npos) end = path.length();
                params.push_back(path.substr(pos + 6, end - pos - 6));
                pos = end;
            }
            regexRoutes_.emplace_back(pattern, params, handler, method);
            
            // 添加Trie路由
            trieRoutes_.addRoute(path, method, handler, params);
        }
    }
    
public:
    void runBenchmark(int numRoutes, int numRequests) {
        std::cout << "生成 " << numRoutes << " 个路由..." << std::endl;
        generateTestData(numRoutes);
        
        std::cout << "\n生成 " << numRequests << " 个测试请求..." << std::endl;
        std::vector<std::pair<std::string, std::string>> testRequests;
        for (int i = 0; i < numRequests; ++i) {
            int depth = std::rand() % 5 + 1;
            testRequests.emplace_back(generateRandomPath(depth), "GET");
        }
        
        // 测试正则匹配
        std::cout << "\n测试正则表达式匹配..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        int regexMatches = 0;
        for (const auto& request : testRequests) {
            for (const auto& route : regexRoutes_) {
                if (route.method != request.second) continue;
                std::smatch matches;
                if (std::regex_match(request.first, matches, route.pattern)) {
                    regexMatches++;
                    break;
                }
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto regexDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        // 测试Trie匹配
        std::cout << "测试Trie树匹配..." << std::endl;
        start = std::chrono::high_resolution_clock::now();
        int trieMatches = 0;
        for (const auto& request : testRequests) {
            auto match = trieRoutes_.findRoute(request.first, request.second);
            if (!match.handler.empty()) {
                trieMatches++;
            }
        }
        end = std::chrono::high_resolution_clock::now();
        auto trieDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        // 输出结果
        std::cout << "\n性能测试结果:" << std::endl;
        std::cout << "正则表达式匹配:" << std::endl;
        std::cout << "  总请求数: " << numRequests << std::endl;
        std::cout << "  匹配成功数: " << regexMatches << std::endl;
        std::cout << "  耗时: " << regexDuration.count() << "ms" << std::endl;
        std::cout << "  平均每个请求耗时: " << static_cast<double>(regexDuration.count()) /static_cast<double>(numRequests) << "ms" << std::endl;
        
        std::cout << "\nTrie树匹配:" << std::endl;
        std::cout << "  总请求数: " << numRequests << std::endl;
        std::cout << "  匹配成功数: " << trieMatches << std::endl;
        std::cout << "  耗时: " << trieDuration.count() << "ms" << std::endl;
        std::cout << "  平均每个请求耗时: " << static_cast<double>(trieDuration.count()) / static_cast<double>(numRequests) << "ms" << std::endl;
        
        std::cout << "\n性能提升: " 
                  << (static_cast<double>(regexDuration.count() - trieDuration.count()) / static_cast<double>(regexDuration.count())) * 100.0
                  << "%" << std::endl;
    }
};

int main() {
    RouteBenchmark benchmark;
    
    // 测试不同规模的路由和请求
    std::cout << "=== 小规模测试 (100路由, 1000请求) ===" << std::endl;
    benchmark.runBenchmark(100, 1000);
    
    std::cout << "\n=== 中等规模测试 (1000路由, 10000请求) ===" << std::endl;
    benchmark.runBenchmark(1000, 10000);
    
    std::cout << "\n=== 大规模测试 (10000路由, 100000请求) ===" << std::endl;
    benchmark.runBenchmark(10000, 100000);
    
    return 0;
}