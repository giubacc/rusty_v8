cmake_minimum_required(VERSION 3.12)
project(bridge_v8 VERSION 0.0.0)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
# add_compile_options(-Wall -std=c++20)
  add_compile_options(-g -Wall -std=c++20)
endif()

add_definitions(-DV8_COMPRESS_POINTERS
                -DV8_ENABLE_SANDBOX)

set(CONTRIB_PATH ".." CACHE STRING "the path where contrib resources are placed")

include_directories(${CONTRIB_PATH}
                    ${CONTRIB_PATH}/v8/include
                    ${CONTRIB_PATH}/spdlog/include
                    ${CONTRIB_PATH}/bridge)

add_library(bridge_v8
            bridge_v8.cpp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
