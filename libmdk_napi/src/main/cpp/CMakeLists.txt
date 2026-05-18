cmake_minimum_required(VERSION 3.17)
project(mdk_napi)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/deps.cmake)
setup_mdk()

add_library(mdk_napi MODULE
    player_napi.cpp
    global_napi.cpp
    media_info_napi.cpp
)

target_link_libraries(mdk_napi
    mdk
    ace_napi.z
    ace_ndk.z
    hilog_ndk.z
    rawfile.z
)