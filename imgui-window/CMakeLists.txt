cmake_minimum_required(VERSION 3.20)

add_library(imgui-window STATIC
	include/imgui-window/Window.h
	include/imgui-window/Dockspace.h
	src/Window.cpp
	src/Dockspace.cpp)

target_include_directories(imgui-window PUBLIC include)

target_link_libraries(imgui-window
	PUBLIC imgui
	PRIVATE SDL2-static)