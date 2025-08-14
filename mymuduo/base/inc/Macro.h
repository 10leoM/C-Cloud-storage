#pragma once

// 禁止拷贝
#define DISALLOW_COPY(cname) \
    cname(const cname &) = delete;            /* NOLINT */ \
    cname &operator=(const cname &) = delete; /* NOLINT */

// 禁止移动
#define DISALLOW_MOVE(cname) \
    cname(cname &&) = delete;            /* NOLINT */ \
    cname &operator=(cname &&) = delete; /* NOLINT */

#define DISALLOW_COPY_AND_MOVE(cname) \
    DISALLOW_COPY(cname);            \
    DISALLOW_MOVE(cname);
