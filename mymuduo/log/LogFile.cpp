#include "LogFile.h"
#include "util.h"

LogFile::LogFile(const char *filepath) : fp_(fopen(filepath, "a+")), written_bytes_(0), lastwrite_(0), lastflush_(0)
{
    if (!fp_) 
    {
        std::string DefaultPath = "../../logs/LogFile_" + TimeStamp::Now().ToFormattedString() + ".log";
        fp_ = fopen(DefaultPath.c_str(), "a+");
    }
    errif(!fp_, "LogFile::LogFile - fopen error");
}

LogFile::~LogFile()
{
    Flush();
    if (fp_)
        fclose(fp_);
}

void LogFile::Write(const char *data, int len)
{
   int pos = 0;
   while(pos != len)
   {
        pos += static_cast<int>(fwrite_unlocked(data + pos, sizeof(char), len - pos, fp_)); // 使用无锁版本加快写入速度，一般只有一个后端线程
   }

   time_t now = time(nullptr);
   if (len != 0) 
   {
       lastwrite_ = now;
       written_bytes_ += len;
   }
   // 判断是否需要Flush
   if (now - lastflush_ > FlushInterval) {
        Flush();
        lastflush_ = now;
    }
}

void LogFile::Flush()
{
    fflush(fp_);
}

int64_t LogFile::GetWrittenBytes() const
{
    return written_bytes_;
}