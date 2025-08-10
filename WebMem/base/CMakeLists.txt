set(base_SRCS
    Timestamp.cc
    Date.cc
    TimeZone.cc
    Exception.cc
    Condition.cc
    CurrentThread.cc
    LogStream.cc
    Logging.cc
    Thread.cc
    CountDownLatch.cc
    ProcessInfo.cc
    FileUtil.cc
    LogFile.cc
    AsyncLogging.cc
    ThreadPool.cc
)

set(base_HEADERS
    Version.h
    Types.h
    copyable.h
    noncopyable.h
    StringPiece.h
    Timestamp.h
    Date.h
    TimeZone.h
    Exception.h
    Atomic.h
    Mutex.h
    Condition.h
    Singleton.h
    ThreadLocal.h
    ThreadLocalSingleton.h
    CurrentThread.h
    LogStream.h
    Logging.h
    Thread.h
    CountDownLatch.h
    ThreadPool.h
    WeakCallback.h
)

# 创建库并设置属性
add_library(mymuduo_base ${base_SRCS})
target_link_libraries(mymuduo_base pthread rt)
target_include_directories(mymuduo_base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# 安装
install(TARGETS mymuduo_base DESTINATION lib)
install(FILES ${base_HEADERS} DESTINATION include/mymuduo/base)

if(NOT CMAKE_BUILD_NO_EXAMPLES)
    # 这里将在后续课程中添加示例程序
endif() 