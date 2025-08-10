#include "TimeZone.h"
#include "Date.h"
#include "noncopyable.h"

#include <string.h>
#include <string>
#include <vector>
#include <algorithm>

namespace mymuduo
{

struct TimeZone::Data
{
    struct Transition
    {
        time_t gmttime;
        time_t localtime;
        int localtimeIdx;

        Transition(time_t t, time_t l, int localIdx)
            : gmttime(t), localtime(l), localtimeIdx(localIdx) {}
    };

    struct LocalTime
    {
        time_t gmtOffset;
        bool isDst;
        int desigIdx;

        LocalTime(time_t offset, bool dst, int idx)
            : gmtOffset(offset), isDst(dst), desigIdx(idx) {}
    };

    void addLocalTime(int utcOffset, bool isDst, int desigIdx)
    {
        localtimes.push_back(LocalTime(utcOffset, isDst, desigIdx));
    }

    void addTransition(time_t when, int localIdx)
    {
        LocalTime& local = localtimes[localIdx];
        transitions.push_back(Transition(when, when + local.gmtOffset, localIdx));
    }

    const LocalTime* findLocalTime(const Transition& trans) const
    {
        return &localtimes[trans.localtimeIdx];
    }

    const LocalTime* findLocalTime(time_t localtime) const;

    std::vector<Transition> transitions;
    std::vector<LocalTime> localtimes;
    std::string abbreviation;
    std::string tzstring;
};

const TimeZone::Data::LocalTime* TimeZone::Data::findLocalTime(time_t localtime) const
{
    const LocalTime* local = nullptr;

    // 二分查找
    if (transitions.empty() || localtime < transitions.front().localtime)
    {
        // 使用第一个本地时间
        local = &localtimes.front();
    }
    else
    {
        Transition sentry(localtime, 0, 0);
        auto trans = std::upper_bound(transitions.begin(),
                                    transitions.end(),
                                    sentry,
                                    [](const Transition& lhs, const Transition& rhs) {
                                        return lhs.localtime < rhs.localtime;
                                    });
        if (trans != transitions.begin())
        {
            --trans;
            local = &localtimes[trans->localtimeIdx];
        }
        else
        {
            local = &localtimes[transitions.front().localtimeIdx];
        }
    }

    return local;
}

TimeZone::TimeZone(const char* zonefile)
{
    // 简化实现：仅支持固定的UTC偏移
    struct timezone
    {
        const char* name;
        int offset;  // 秒
    };

    static const timezone zones[] = {
        {"Asia/Shanghai", 8 * 3600},
        {"Asia/Tokyo", 9 * 3600},
        {"America/New_York", -5 * 3600},
        {"Europe/London", 0},
        {"UTC", 0},
    };

    for (const auto& z : zones)
    {
        if (strcmp(zonefile, z.name) == 0)
        {
            data_ = std::make_shared<Data>();
            data_->addLocalTime(z.offset, false, 0);
            data_->tzstring = z.name;
            break;
        }
    }
}

TimeZone::TimeZone(int eastOfUtc, const char* name)
{
    data_ = std::make_shared<Data>();
    data_->addLocalTime(eastOfUtc, false, 0);
    data_->tzstring = name;
}

struct tm TimeZone::toLocalTime(time_t seconds) const
{
    struct tm localTime;
    memZero(&localTime, sizeof(localTime));
    
    if (!data_)
    {
        // 无效时区，返回UTC时间
        gmtime_r(&seconds, &localTime);
        return localTime;
    }

    const Data::LocalTime* local = data_->findLocalTime(seconds);
    time_t localSeconds = seconds + local->gmtOffset;
    gmtime_r(&localSeconds, &localTime);
    localTime.tm_isdst = local->isDst;
    return localTime;
}

time_t TimeZone::fromLocalTime(const struct tm& localTm) const
{
    if (data_)
    {
        time_t local = mktime(const_cast<struct tm*>(&localTm));
        const Data::LocalTime* localTime = data_->findLocalTime(local);
        return local - localTime->gmtOffset;
    }
    
    // 无效时区，假设是UTC时间
    return mktime(const_cast<struct tm*>(&localTm));
}

struct tm TimeZone::utcTime(time_t secondsSinceEpoch)
{
    struct tm utc;
    gmtime_r(&secondsSinceEpoch, &utc);
    return utc;
}

time_t TimeZone::fromUtcTime(const struct tm& utc)
{
    time_t seconds = timegm(const_cast<struct tm*>(&utc));
    return seconds;
}

}  // namespace mymuduo 