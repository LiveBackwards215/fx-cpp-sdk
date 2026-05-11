cmake_minimum_required(VERSION 3.20)
project(example LANGUAGES CXX)

add_executable(example server.cpp)
target_include_directories(example PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_features(example PUBLIC cxx_std_23)
target_link_options(example PRIVATE -static-libgcc -static-libstdc++)
