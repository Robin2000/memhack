cmake_minimum_required(VERSION 3.10)

project(MemoryMonitor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置Windows SDK路径
if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /W4 /permissive-")
    include_directories($ENV{WindowsSdkDir}Include)
    link_directories($ENV{WindowsSdkDir}Lib)
endif()

add_executable(MemoryMonitor main.cpp)

# 根据平台设置编译选项和链接库
if(WIN32)
    target_link_libraries(MemoryMonitor 
        kernel32.lib
        user32.lib
        psapi.lib
        advapi32.lib
    )
elseif(UNIX)
    target_link_libraries(MemoryMonitor pthread)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g") # 添加编译警告和调试信息
endif()
