#include "Util.h"
#include <random>
#include <cstring>
#include <chrono>
#include <algorithm>

std::string RandomString(size_t len, const char *charset)
{
    std::random_device rd;                                           // 随机数生成器
    std::mt19937 gen(rd());                                          // Mersenne Twister 引擎
    size_t n = strlen(charset);                                      // 字符集长度
    std::uniform_int_distribution<> dis(0, static_cast<int>(n - 1)); // 均匀分布
    std::string out;
    out.reserve(len);
    for (size_t i = 0; i < len; ++i)
        out += charset[dis(gen)]; // 从字符集中随机选择字符
    return out;
}

std::string PseudoSha256(const std::string &input)
{
    std::hash<std::string> hasher; // 哈希函数
    size_t hash = hasher(input);   // 计算哈希值
    std::stringstream ss;
    ss << std::hex << hash; // 转换为十六进制字符串
    return ss.str();
}

std::string UrlDecode(const std::string &encoded) // URL 解码（与原有实现等价）
{
    std::string result;
    char ch;
    size_t i;
    int ii;
    size_t len = encoded.length();

    for (int i = 0; i < len; i++)
    {
        if (encoded[i] != '%')
        {
            if (encoded[i] == '+')
                result += ' ';
            else
                result += encoded[i];
        }
        else
        {
            sscanf(encoded.substr(i + 1, 2).c_str(), "%x", &ii);
            ch = static_cast<char>(ii);
            result += ch;
            i = i + 2;
        }
    }
    return result;
}

std::string UniqueFilename(const std::string &prefix)  // 基于时间戳+随机数的唯一文件名
{
    auto now = std::chrono::system_clock::now();
    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return prefix + "_" + std::to_string(ts) + "_" + RandomString(4);
}

std::string FileTypeByExt(const std::string &filename) // 简单判断文件类型，MIME
{
    size_t dotPos = filename.find_last_of('.');
    if(dotPos!=std::string::npos && dotPos < filename.length() - 1) 
    {
        std::string ext = filename.substr(dotPos + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower); // 转为小写
        if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "gif") return "image";
        if (ext == "mp4" || ext == "avi" || ext == "mov" || ext == "wmv") return "video";
        if (ext == "pdf") return "pdf";
        if (ext == "doc" || ext == "docx") return "word";
        if (ext == "xls" || ext == "xlsx") return "excel";
        if (ext == "ppt" || ext == "pptx") return "powerpoint";
        if (ext == "txt" || ext == "csv") return "text";
        return "other";
    }
    return "unknown";
}
