cmake_minimum_required(VERSION 3.10)

project(MemoryMonitor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 根据平台设置编译选项和链接库
if(WIN32)
    target_link_libraries(MemoryMonitor kernel32)
elseif(UNIX)
    target_link_libraries(MemoryMonitor pthread)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g") # 添加编译警告和调试信息
endif()

add_executable(MemoryMonitor main.cpp)