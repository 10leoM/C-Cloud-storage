#pragma once 

// 全局配置，数据库，端口等
#include <string>
#include <cstdint>

class Config {
public:
    Config(){}
    ~Config(){}
    static Config& GetInstance() 
    {
        static Config instance;
        return instance;
    }

    // 获取配置
    std::string GetDbHost() const { return db_host_; }
    int GetDbPort() const { return db_port_; }
    std::string GetDbUser() const { return db_user_; }
    std::string GetDbPassword() const { return db_password_; }
    std::string GetDbName() const { return db_name_; }
    std::string GetServerIp() const { return server_ip_; }
    int GetServerPort() const { return server_port_; }

    // 设置配置
    void SetDbHost(const std::string& host) { db_host_ = host; }
    void SetDbPort(int port) { db_port_ = port; }
    void SetDbUser(const std::string& user) { db_user_ = user; }
    void SetDbPassword(const std::string& password) { db_password_ = password; }
    void SetDbName(const std::string& name) { db_name_ = name; }
    void SetServerIp(const std::string& ip) { server_ip_ = ip; }
    void SetServerPort(int port) { server_port_ = port; }

private:
    std::string db_host_ = "localhost";
    int db_port_ = 3306;
    std::string db_user_ = "root";
    std::string db_password_ = "123456";
    std::string db_name_ = "chat";
    std::string server_ip_ = "127.0.0.1";
    int server_port_ = 8080;
};