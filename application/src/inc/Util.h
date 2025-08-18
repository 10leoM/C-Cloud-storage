#pragma once

// 通用工具,URL解码，随机串，MIME/后缀

#include <string>
#include <vector>

std::string RandomString(size_t len, const char *charset = "abcdefghijklmnopqrstuvwxyz0123456789"); // 生成指定长度的随机字符串，字符集默认 [a-z0-9]
std::string PseudoSha256(const std::string &input);                                                 // 伪 sha256（与原有实现等价：std::hash 十六进制）
std::string UrlDecode(const std::string &encoded);                                                  // URL 解码（与原有实现等价）
std::string UniqueFilename(const std::string &prefix);                                              // 基于时间戳+随机数的唯一文件名
std::string FileTypeByExt(const std::string &filename);                                             // 简单判断文件类型（按扩展名）
