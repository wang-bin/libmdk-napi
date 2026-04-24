cmake_minimum_required(VERSION 3.17)
project(mdk_napi)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/deps.cmake)
setup_mdk()

add_library(mdk_napi SHARED
    player_napi.cpp
)

target_link_libraries(mdk_napi
    mdk
    libace_napi.z.so
    libace_ndk.z.so
    libhilog_ndk.z.so
    libohfileuri.so
)