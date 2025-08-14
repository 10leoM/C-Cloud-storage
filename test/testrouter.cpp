#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include "RouterTrie.h"

struct Check {
    int fails = 0;
    void expect(bool cond, const std::string &msg) {
        if (!cond) {
            ++fails;
            std::cout << "[FAIL] " << msg << "\n";
        } else {
            std::cout << "[ OK ] " << msg << "\n";
        }
    }
};

int main() {
    RouteTrie router;
    Check ck;

    // 注册路由
    router.addRoute("/hello", "GET", "Hello");
    router.addRoute("/users/:id", "GET", "UserShow");
    router.addRoute("/static/**", "GET", "StaticCatchAll");

    // 1) 静态命中
    {
        RouteMatch m = router.findRoute("/hello", "GET");
        ck.expect(m.handler == "Hello", "GET /hello -> Hello");
        ck.expect(m.params.empty(), "GET /hello has no params");
    }

    // 2) 方法不匹配（当前实现即视为未命中）
    {
        RouteMatch m = router.findRoute("/hello", "POST");
        ck.expect(m.handler.empty(), "POST /hello -> not found");
    }

    // 3) 参数匹配
    {
        RouteMatch m = router.findRoute("/users/42", "GET");
        ck.expect(m.handler == "UserShow", "GET /users/42 -> UserShow");
        ck.expect(m.params.count("id") && m.params["id"] == "42", "param id = 42");
    }

    // 4) 查询串剥离
    {
        RouteMatch m = router.findRoute("/users/99?foo=bar&x=1", "GET");
        ck.expect(m.handler == "UserShow", "GET /users/99? -> UserShow");
        ck.expect(m.params.count("id") && m.params["id"] == "99", "param id = 99 with query");
    }

    // 5) 通配符 ** 捕获
    {
        RouteMatch m = router.findRoute("/static/js/app.js", "GET");
        ck.expect(m.handler == "StaticCatchAll", "GET /static/js/app.js -> StaticCatchAll");
        // 本实现把 ** 捕获保存在 params["*"]
        ck.expect(m.params.count("*") && m.params["*"] == "js/app.js", "wildcard ** captured as 'js/app.js'");
    }

    // 6) 未命中路径
    {
        RouteMatch m = router.findRoute("/nope", "GET");
        ck.expect(m.handler.empty(), "GET /nope -> not found");
    }

    std::cout << "\nSummary: " << (ck.fails == 0 ? "ALL PASSED" : std::to_string(ck.fails) + " FAILED") << "\n";
    return ck.fails == 0 ? 0 : 1;
}
