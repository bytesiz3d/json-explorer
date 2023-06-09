cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

project(json-explorer LANGUAGES C CXX)

set(MASTER_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
	set(MASTER_PROJECT ON)
endif ()

option(JSON_EXPLORER_BUILD_TESTS "Build tests" ${MASTER_PROJECT})
option(JSON_EXPLORER_PROFILE "Build profiler" ${MASTER_PROJECT})
set(TRACY_ENABLE ${JSON_EXPORER_PROFILE})

include(FetchContent)

FetchContent_Declare(utf8proc
	GIT_REPOSITORY git@github.com:JuliaStrings/utf8proc.git
	GIT_SHALLOW TRUE
	GIT_TAG v2.8.0)

FetchContent_Declare(imgui
	GIT_REPOSITORY git@github.com:ocornut/imgui.git
	GIT_SHALLOW TRUE
	GIT_TAG docking)

FetchContent_Declare(sokol
	GIT_REPOSITORY git@github.com:floooh/sokol.git
	GIT_SHALLOW TRUE
	GIT_TAG master)

FetchContent_Declare(tracy
	GIT_REPOSITORY git@github.com:wolfpld/tracy.git
	GIT_SHALLOW TRUE
	GIT_TAG v0.9.1)

FetchContent_MakeAvailable(utf8proc imgui sokol tracy)
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
	${imgui_SOURCE_DIR}/imgui_widgets.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})

add_library(sokol INTERFACE
	${sokol_SOURCE_DIR}/sokol_app.h
	${sokol_SOURCE_DIR}/sokol_gfx.h
	${sokol_SOURCE_DIR}/sokol_glue.h
	${sokol_SOURCE_DIR}/util/sokol_imgui.h
	${sokol_SOURCE_DIR}/sokol_log.h)
target_include_directories(sokol INTERFACE ${sokol_SOURCE_DIR})

add_subdirectory(json-parser)
add_subdirectory(json-explorer)

if (${JSON_EXPLORER_BUILD_TESTS})
	FetchContent_Declare(doctest
		GIT_REPOSITORY git@github.com:doctest/doctest.git
		GIT_SHALLOW TRUE
		GIT_TAG v2.4.11)

	FetchContent_Declare(json-test-suite
		GIT_REPOSITORY git@github.com:nst/JSONTestSuite.git
		GIT_SHALLOW TRUE
		GIT_TAG master)

	FetchContent_MakeAvailable(doctest json-test-suite)

	list(APPEND CMAKE_MODULE_PATH ${doctest_SOURCE_DIR}/scripts/cmake)
	include(CTest)
	enable_testing()
	add_subdirectory(json-parser-tests)
endif()

if (${JSON_EXPLORER_PROFILE})
	add_subdirectory(json-parser-profile)
endif()
