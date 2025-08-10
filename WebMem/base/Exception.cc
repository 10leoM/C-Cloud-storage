#include "Exception.h"

#include <execinfo.h>  // for backtrace
#include <stdlib.h>    // for malloc/free
#include <stdio.h>     // for snprintf

namespace mymuduo
{

Exception::Exception(const char* msg)
    : message_(msg)
{
    fillStackTrace();
}

Exception::Exception(const std::string& msg)
    : message_(msg)
{
    fillStackTrace();
}

void Exception::fillStackTrace()
{
    const int len = 200;
    void* buffer[len];
    int nptrs = ::backtrace(buffer, len);
    char** strings = ::backtrace_symbols(buffer, nptrs);
    if (strings)
    {
        for (int i = 0; i < nptrs; ++i)
        {
            // TODO: demangle function names with abi::__cxa_demangle
            stack_.append(strings[i]);
            stack_.push_back('\n');
        }
        free(strings);
    }
}

}  // namespace mymuduo 