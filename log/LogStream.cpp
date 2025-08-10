#include "LogStream.h"
#include <string.h>


// LogStream 实现
LogStream::LogStream() {}
LogStream::~LogStream() {}

void LogStream::append(const char *data, int len) 
{
    buffer_.append(data, len);
}
const FixedBuffer<FixedBufferSize> &LogStream::GetBuffer() const { return buffer_; }
void LogStream::resetBuffer()  { buffer_.reset(); }

LogStream &LogStream::operator<<(bool v) { buffer_.append(v ? "1" : "0", 1); return *this; }
LogStream &LogStream::operator<<(short num) { return (*this) << static_cast<int>(num); }                    // 调用下面的重载
LogStream &LogStream::operator<<(unsigned short num) { return (*this) << static_cast<unsigned int>(num); } // 调用下面的重载
LogStream &LogStream::operator<<(int num) { formatInteger(num); return *this; }
LogStream &LogStream::operator<<(unsigned int num) { formatInteger(num); return *this; }
LogStream &LogStream::operator<<(long num) { formatInteger(num); return *this; }
LogStream &LogStream::operator<<(unsigned long num) { formatInteger(num); return *this; }
LogStream &LogStream::operator<<(long long num) { formatInteger(num); return *this; }
LogStream &LogStream::operator<<(unsigned long long num) { formatInteger(num); return *this; }

LogStream &LogStream::operator<<(const float &num) { return (*this) << static_cast<const double>(num); } // 调用下面的重载
LogStream &LogStream::operator<<(const double &num) 
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%g", num);
    append(buf, strlen(buf));
    return *this;
}

LogStream &LogStream::operator<<(char v)
{
    buffer_.append(&v, 1);
    return *this;
}
LogStream &LogStream::operator<<(const char *str)
{
    if(str)
        buffer_.append(str, strlen(str));
    else
        buffer_.append("(null)", 6);
    return *this;
}

LogStream &LogStream::operator<<(const std::string &v)
{
    buffer_.append(v.c_str(), v.size());
    return *this;
}
