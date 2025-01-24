cmake_minimum_required(VERSION 3.10)

project(MemoryMonitor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 璁剧疆Windows SDK璺緞
if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /W4 /permissive-")
    include_directories($ENV{WindowsSdkDir}Include)
    link_directories($ENV{WindowsSdkDir}Lib)
endif()

# Keystone 寮曟搸閰嶇疆
set(KEYSTONE_ROOT "D:/Program/keystone-0.9.2-win32")
include_directories(${KEYSTONE_ROOT}/include/keystone)
link_directories(${KEYSTONE_ROOT}/lib)

add_executable(MemoryMonitor main.cpp)

# 閾炬帴Keystone搴?
target_link_libraries(MemoryMonitor ${KEYSTONE_ROOT}/lib/keystone.lib)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR})

# Override Visual Studio's default output directories
set_property(TARGET MemoryMonitor PROPERTY RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set_property(TARGET MemoryMonitor PROPERTY RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})
set_property(TARGET MemoryMonitor PROPERTY RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR})

# 鏍规嵁骞冲彴璁剧疆缂栬瘧閫夐」鍜岄摼鎺ュ簱
if(WIN32)
    target_link_libraries(MemoryMonitor 
        kernel32.lib
        user32.lib
        psapi.lib
        advapi32.lib
    )
elseif(UNIX)
    target_link_libraries(MemoryMonitor pthread)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g") # 娣诲姞缂栬瘧璀﹀憡鍜岃皟璇曚俊鎭?
endif()
