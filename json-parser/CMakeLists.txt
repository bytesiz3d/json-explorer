cmake_minimum_required(VERSION 3.20)

add_library(json-parser SHARED
	include/json-parser/json-parser.h
	src/json-parser.cpp)
target_include_directories(json-parser PUBLIC include)

include(GenerateExportHeader)
generate_export_header(json-parser EXPORT_FILE_NAME ${CMAKE_CURRENT_SOURCE_DIR}/include/json-parser/Exports.h)
