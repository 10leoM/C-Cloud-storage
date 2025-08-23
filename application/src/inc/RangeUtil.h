#pragma once

#include <string>
#include <cstdint>
#include <regex>

namespace HttpRange
{
    struct RangeSpec // Range 解析
    {
        bool isRange{false};    // 是否包含 Range 头
        uint64_t start{0};      // 范围开始
        uint64_t end{0};        // 范围结束
        bool satisfiable{true}; // 范围是否可满足
    };

    // Parse HTTP Range header like: "bytes=start-end" or "bytes=start-"
    // Only supports a single range. If invalid or missing, returns {isRange=false}.
    inline RangeSpec parse(const std::string &header, uint64_t fileSize)
    {
        RangeSpec spec;
        if(header.empty() || fileSize == 0)
        {
            spec.isRange = false;
            spec.satisfiable = true;
            return spec;
        }
        std::regex re("bytes=(\\d+)-(\\d*)");       // 正则匹配 Range 头
        std::smatch m;
        if(!std::regex_search(header, m, re))       // 未匹配到 Range 头
        {
            spec.isRange = false;
            spec.satisfiable = true;
            return spec;
        }
        uint64_t start = 0;
        uint64_t end = fileSize > 0 ? fileSize - 1 : 0;
        try
        {
            start = m[1].str().empty() ? 0ULL : static_cast<uint64_t>(std::stoull(m[1].str()));
            if (!m[2].str().empty())
                end = static_cast<uint64_t>(std::stoull(m[2].str()));
        }
        catch (...)
        {
            spec.isRange = false;
            spec.satisfiable = true;
            return spec;
        }

        if(start >= fileSize) // 开始位置超出文件大小
        {
            spec.isRange = true;
            spec.satisfiable = false;
            return spec;
        }
        if(end >= fileSize) // 结束位置超出文件大小
            end = fileSize - 1;
        if(end < start) // 结束位置小于开始位置
        {
            spec.isRange = true;
            spec.satisfiable = false;
            return spec;
        }
        spec.isRange = true;
        spec.start = start;
        spec.end = end;
        spec.satisfiable = true;
        return spec;
    }
}