#include "util.h"
#include <stdlib.h>
#include <stdio.h>
#include <cassert>

void errif(bool condition, const char *msg)
{
    if(condition)
    {
        perror(msg);
        assert(!condition); // 触发断言，终止程序
        // exit(EXIT_FAILURE);
    }
}