cmake_minimum_required(VERSION 3.22.1)
project(meshly_native CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(meshly_native SHARED
    native_lib.cpp
    renderer.cpp)

find_library(log-lib log)
target_link_libraries(meshly_native GLESv3 ${log-lib})

# 16 KB page alignment (required by newer Android versions / Google Play).
target_link_options(meshly_native PRIVATE -Wl,-z,max-page-size=16384)
