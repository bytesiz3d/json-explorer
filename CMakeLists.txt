cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

project(json-explorer LANGUAGES C CXX)

option(JSON_EXPLORER_BUILD_TESTS "Build tests")

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
add_subdirectory(json-parser)
add_subdirectory(json-explorer)

if (${JSON_EXPLORER_BUILD_TESTS})
	FetchContent_Declare(doctest
		GIT_REPOSITORY git@github.com:doctest/doctest.git
		GIT_TAG v2.4.11)

	FetchContent_Declare(json-test-suite
		GIT_REPOSITORY git@github.com:nst/JSONTestSuite.git
		GIT_TAG master)
	
	FetchContent_MakeAvailable(doctest json-test-suite)

	list(APPEND CMAKE_MODULE_PATH ${doctest_SOURCE_DIR}/scripts/cmake)
	include(CTest)
	enable_testing()
	add_subdirectory(json-parser-tests)
endif()
