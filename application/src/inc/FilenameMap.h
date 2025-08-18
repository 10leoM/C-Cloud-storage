#pragma once

#include <string>
#include <map>
#include <mutex>

class FilenameMap
{
public:
    explicit FilenameMap(const std::string &mappingFile);

    void load(); // 加载映射
    void save(); // 保存映射

    void add(const std::string &serverFilename, const std::string &originalFilename); // 添加映射
    std::string getOriginal(const std::string &serverFilename);                       // 获取原始文件名
    std::map<std::string, std::string> getAll();                                      // 获取所有映射
    void erase(const std::string &serverFilename);                                    // 删除映射

private:
    void loadInternal(); // 内部加载实现
    void saveInternal(); // 内部保存实现

    std::string mappingFile_;                    // 映射文件路径
    std::mutex mtx_;                             // 互斥锁保护
    std::map<std::string, std::string> mapping_; // 映射关系：服务器文件名 -> 原始文件名
};