cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 17)

project(imgui-starter
	LANGUAGES C CXX
)

include(FetchContent)

FetchContent_Declare(imgui
	GIT_REPOSITORY git@github.com:ocornut/imgui.git
	GIT_TAG docking)

FetchContent_Declare(SDL2
	GIT_REPOSITORY git@github.com:libsdl-org/SDL.git
	GIT_TAG release-2.26.5)
set(SDL_SHARED OFF)
FetchContent_MakeAvailable(imgui SDL2)

add_library(imgui STATIC
	${imgui_SOURCE_DIR}/imconfig.h
	${imgui_SOURCE_DIR}/imgui.h
	${imgui_SOURCE_DIR}/imgui_internal.h
	${imgui_SOURCE_DIR}/imstb_rectpack.h
	${imgui_SOURCE_DIR}/imstb_textedit.h
	${imgui_SOURCE_DIR}/imstb_truetype.h
	#
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_demo.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
	${imgui_SOURCE_DIR}/imgui_tables.cpp
	${imgui_SOURCE_DIR}/imgui_widgets.cpp
	# 
	${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.h
	${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer.h
	${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer.cpp)

target_link_libraries(imgui SDL2-static)

target_include_directories(imgui
	PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)

add_subdirectory(imgui-window)
add_executable(imgui-starter imgui-starter.cpp)
target_link_libraries(imgui-starter imgui-window)