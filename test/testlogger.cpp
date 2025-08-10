#include "Logger.h"
#include "LogStream.h"
#include "AsyncLogging.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <climits>
#include <filesystem>

// 全局异步日志实例
std::unique_ptr<AsyncLogging> g_asyncLog;

// 异步输出函数
void AsyncOutputFunc(const char *data, int len) {
    if (g_asyncLog) {
        g_asyncLog->Append(data, len);
    }
}

// 异步刷新函数
void AsyncFlushFunc() {
    if (g_asyncLog) {
        g_asyncLog->Flush();
    }
}

// 初始化异步日志系统
void initAsyncLogging(const std::string& logFileName = "test_async.log") {
    std::cout << "🚀 初始化异步日志系统: " << logFileName << "\n";
    
    // 创建日志目录
    std::filesystem::create_directories("logs");
    
    // 创建异步日志实例
    std::string logPath = "logs/" + logFileName;
    g_asyncLog = std::make_unique<AsyncLogging>(logPath.c_str());
    
    // 设置Logger的输出函数为异步输出
    Logger::SetOutput(AsyncOutputFunc);
    Logger::SetFlush(AsyncFlushFunc);
    
    // 启动异步日志后台线程
    g_asyncLog->Start();
    
    std::cout << "✅ 异步日志系统初始化完成\n";
}

// 停止异步日志系统
void stopAsyncLogging() {
    if (g_asyncLog) {
        std::cout << "📝 停止异步日志系统...\n";
        g_asyncLog->Stop();
        g_asyncLog.reset();
        
        // 恢复默认输出（可选）
        Logger::SetOutput(nullptr);
        Logger::SetFlush(nullptr);
        std::cout << "✅ 异步日志系统已停止\n";
    }
}

// 测试基本日志功能
void testBasicLogging() {
    std::cout << "\n=== 🧪 基本日志功能测试 ===\n";
    
    LOG_INFO << "=== 异步日志库基本功能测试开始 ===";
    
    // 测试不同级别的日志
    LOG_INFO << "这是INFO级别日志 - 一般信息";
    LOG_WARN << "这是WARN级别日志 - 警告信息";
    LOG_ERROR << "这是ERROR级别日志 - 错误信息";
    
    // 测试不同数据类型
    int user_id = 12345;
    double response_time = 15.678;
    std::string user_name = "张三";
    char status = 'A';
    bool is_admin = true;
    
    LOG_INFO << "用户登录测试: "
             << "ID=" << user_id
             << ", 姓名=" << user_name
             << ", 响应时间=" << response_time << "ms"
             << ", 状态=" << status
             << ", 管理员=" << (is_admin ? "是" : "否");
    
    // 测试格式化输出
    LOG_INFO << "格式化测试: "
             << "十六进制=" << Fmt("0x%X", 255)
             << ", 浮点精度=" << Fmt("%.2f", 3.14159)
             << ", 零填充=" << Fmt("%08d", 42);
    
    LOG_INFO << "基本功能测试完成，所有日志已异步写入文件";
}

// 测试日志级别过滤
void testLogLevelFiltering() {
    std::cout << "\n=== 🎚️ 日志级别过滤测试 ===\n";
    
    LOG_INFO << "=== 日志级别过滤测试开始 ===";
    
    // 设置不同的日志级别
    Logger::LogLevel levels[] = {
        Logger::DEBUG, Logger::INFO, Logger::WARN, Logger::ERROR
    };
    
    const char* level_names[] = {
        "DEBUG", "INFO", "WARN", "ERROR"
    };
    
    for (int i = 0; i < 4; ++i) {
        std::cout << "\n--- 设置日志级别为: " << level_names[i] << " ---\n";
        Logger::SetLogLevel(levels[i]);
        
        LOG_INFO << "当前日志级别设置为: " << level_names[i];
        LOG_INFO << "INFO消息 - 应该在INFO及以上级别显示";
        LOG_WARN << "WARN消息 - 应该在WARN及以上级别显示";
        LOG_ERROR << "ERROR消息 - 应该在ERROR及以上级别显示";
    }
    
    // 恢复默认级别
    Logger::SetLogLevel(Logger::INFO);
    LOG_INFO << "恢复默认日志级别: INFO";
}

// 模拟业务场景
class UserService {
public:
    bool login(const std::string& username, const std::string& password) {
        LOG_INFO << "用户尝试登录: " << username;
        
        if (username.empty()) {
            LOG_WARN << "登录失败: 用户名为空";
            return false;
        }
        
        if (password.length() < 6) {
            LOG_ERROR << "登录失败: 密码长度不足, 用户=" << username 
                      << ", 长度=" << password.length();
            return false;
        }
        
        // 模拟数据库查询延迟
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        LOG_INFO << "用户登录成功: " << username;
        return true;
    }
    
    void processRequest(int request_id) {
        auto start = std::chrono::high_resolution_clock::now();
        
        LOG_INFO << "开始处理请求: ID=" << request_id;
        
        // 模拟业务处理
        std::this_thread::sleep_for(std::chrono::milliseconds(5 + request_id % 20));
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        LOG_INFO << "请求处理完成: ID=" << request_id 
                 << ", 耗时=" << duration.count() << "ms";
    }
};

// 测试业务场景
void testBusinessScenario() {
    std::cout << "\n=== 💼 业务场景测试 ===\n";
    
    LOG_INFO << "=== 业务场景测试开始 ===";
    
    UserService service;
    
    // 测试正常登录
    service.login("admin", "password123");
    service.login("user1", "abc123");
    
    // 测试异常情况
    service.login("", "password");
    service.login("user2", "123");  // 密码太短
    
    // 测试请求处理
    LOG_INFO << "开始批量请求处理测试";
    for (int i = 1; i <= 5; ++i) {
        service.processRequest(i);
    }
    
    LOG_INFO << "业务场景测试完成";
}

// 多线程工作函数
void workerThread(int thread_id, int message_count) {
    LOG_INFO << "工作线程 " << thread_id << " 启动，准备发送 " << message_count << " 条消息";
    
    for (int i = 0; i < message_count; ++i) {
        LOG_INFO << "线程" << thread_id << " 消息" << i 
                 << " - 异步日志多线程测试";
        
        if (i % 10 == 0) {
            LOG_WARN << "线程" << thread_id << " 周期性警告: " << i;
        }
        
        // 减少sleep时间，增加并发压力
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
    
    LOG_INFO << "工作线程 " << thread_id << " 完成所有消息发送";
}

// 测试多线程日志
void testMultiThreadLogging() {
    std::cout << "\n=== 🧵 多线程异步日志测试 ===\n";
    
    LOG_INFO << "=== 多线程异步日志测试开始 ===";
    
    const int thread_count = 4;
    const int messages_per_thread = 50;
    
    LOG_INFO << "启动 " << thread_count << " 个线程，每个线程发送 " << messages_per_thread << " 条消息";
    
    std::vector<std::thread> threads;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(workerThread, i + 1, messages_per_thread);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    LOG_INFO << "多线程测试完成: " << thread_count << "个线程, "
             << "每个线程" << messages_per_thread << "条消息, "
             << "总耗时" << duration.count() << "ms";
}

// 性能压力测试
void testPerformance() {
    std::cout << "\n=== ⚡ 异步日志性能测试 ===\n";
    
    LOG_INFO << "=== 异步日志性能测试开始 ===";
    
    const int test_count = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < test_count; ++i) {
        LOG_INFO << "性能测试消息 " << i << ": value=" << i * 3.14 << ", status=OK";
        
        // 每1000条消息输出一次进度
        if (i % 1000 == 0) {
            LOG_WARN << "性能测试进度: " << i << "/" << test_count;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    LOG_INFO << "性能测试完成: 共 " << test_count << " 条消息, "
             << "耗时 " << duration.count() << " 微秒";
    
    std::cout << "📊 性能测试结果:\n";
    std::cout << "   日志条数: " << test_count << "\n";
    std::cout << "   总耗时: " << duration.count() << " 微秒\n";
    std::cout << "   平均耗时: " << duration.count() / test_count << " 微秒/条\n";
    std::cout << "   吞吐量: " << test_count * 1000000 / duration.count() << " 条/秒\n";
}

// 测试边界情况
void testEdgeCases() {
    std::cout << "\n=== 🔍 边界情况测试 ===\n";
    
    LOG_INFO << "=== 边界情况测试开始 ===";
    
    // 测试空字符串
    LOG_INFO << "空字符串测试: '" << "" << "'";
    
    // 测试null指针
    const char* null_ptr = nullptr;
    LOG_INFO << "null指针测试: " << null_ptr;
    
    // 测试很长的字符串
    std::string long_string(1000, 'X');
    LOG_INFO << "长字符串测试(1000字符): " << long_string.substr(0, 50) << "...(省略950字符)";
    
    // 测试特殊字符
    LOG_INFO << "特殊字符测试: 制表符[TAB] 引号[\"] 反斜杠[\\]";
    
    // 测试极值
    LOG_INFO << "数值极值测试: "
             << "int_max=" << INT_MAX
             << ", int_min=" << INT_MIN
             << ", double_max=" << std::numeric_limits<double>::max()
             << ", double_min=" << std::numeric_limits<double>::min();
    
    LOG_INFO << "边界情况测试完成";
}

// 测试异步日志的批量写入特性
void testBatchWriting() {
    std::cout << "\n=== 📦 批量写入特性测试 ===\n";
    
    LOG_INFO << "=== 批量写入特性测试开始 ===";
    
    // 快速生成大量日志，测试缓冲区交换
    const int batch_size = 1000;
    
    for (int batch = 0; batch < 5; ++batch) {
        LOG_INFO << "开始批次 " << batch << " 的快速日志生成";
        
        for (int i = 0; i < batch_size; ++i) {
            LOG_INFO << "批次" << batch << " 消息" << i 
                     << " - 测试异步日志缓冲区交换机制";
        }
        
        LOG_INFO << "完成批次 " << batch << " 共 " << batch_size << " 条消息";
        
        // 短暂暂停让后台线程处理
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    LOG_INFO << "批量写入特性测试完成";
}

int main() {
    std::cout << "🚀 异步日志库完整功能测试开始\n";
    std::cout << std::string(60, '=') << "\n";
    
    try {
        // 初始化异步日志系统
        initAsyncLogging("complete_test.log");
        
        // 1. 基本功能测试
        testBasicLogging();
        
        // 2. 日志级别过滤测试
        testLogLevelFiltering();
        
        // 3. 业务场景测试
        testBusinessScenario();
        
        // 4. 多线程测试
        testMultiThreadLogging();
        
        // 5. 边界情况测试
        testEdgeCases();
        
        // 6. 批量写入特性测试
        testBatchWriting();
        
        // 7. 性能测试（放在最后）
        testPerformance();
        
        // 等待所有日志写入完成
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
    } catch (const std::exception& e) {
        LOG_ERROR << "测试过程中发生异常: " << e.what();
        std::cerr << "异常: " << e.what() << std::endl;
    }
    
    // 停止异步日志系统
    stopAsyncLogging();
    
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "✅ 所有测试完成！\n";
    
    return 0;
}