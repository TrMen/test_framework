cmake_minimum_required(VERSION 3.15)
project(test_framework VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

add_subdirectory(../fmt build)

add_executable(test_framework main.cpp)

target_link_libraries(test_framework PUBLIC fmt::fmt-header-only)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

