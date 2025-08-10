#include "LogStream.h"
#include "Logger.h"
#include <iostream>

int main(){

    LogStream os;
    os << "hello world";
    std::cout << os.GetBuffer().GetData() << std::endl;
    os.resetBuffer();

    os << 11;
    std::cout << os.GetBuffer().GetData() << std::endl;
    os.resetBuffer();

    os << 0.1;
    std::cout << os.GetBuffer().GetData() << std::endl;
    os.resetBuffer();

    os << 128.1281281281281;
    std::cout << os.GetBuffer().GetData() << std::endl;
    os.resetBuffer();

    os << Fmt("%0.5f", 0.1);
    std::cout << os.GetBuffer().GetData() << std::endl;
    os.resetBuffer();

    char buf[4];
    int len = snprintf(buf, sizeof(buf), "%s", "42");
    std::cout<< len << " " << buf << " " << sizeof(buf) << std::endl; // 输出: 2 42

    LOG_INFO << "大傻逼";

    return 0;
}