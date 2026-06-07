cmake_minimum_required(VERSION 3.20)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
project(SMAAT LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(smaat
    src/main.cpp
    src/Utils.cpp
    src/OS.cpp
)

target_include_directories(smaat PRIVATE include)

if (WIN32)
    target_link_libraries(smaat PRIVATE psapi)
endif()
