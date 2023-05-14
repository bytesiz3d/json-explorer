cmake_minimum_required(VERSION 3.20)

add_executable(json-explorer json-explorer.cpp)
target_link_libraries(json-explorer sokol imgui json-parser)

