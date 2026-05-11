cmake_minimum_required(VERSION 3.20)
project(fivem-cpp-sdk LANGUAGES CXX)

option(EXAMPLES "Build example resources" ON)
add_library(fivem-cpp-sdk INTERFACE)
target_include_directories(fivem-cpp-sdk INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(fivem-cpp-sdk INTERFACE cxx_std_23)

if (EXAMPLES)
    add_subdirectory(example)
    target_link_libraries(example PRIVATE fivem-cpp-sdk)
endif()
