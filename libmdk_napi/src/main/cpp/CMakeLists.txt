cmake_minimum_required(VERSION 3.4.1)
project(mdk_napi)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Vendored MDK SDK root. Override with -DMDK_SDK_DIR=/path/to/mdk-sdk if needed.
if(NOT MDK_SDK_DIR)
    set(MDK_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mdk-sdk)
endif()

include(${MDK_SDK_DIR}/lib/cmake/FindMDK.cmake)

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