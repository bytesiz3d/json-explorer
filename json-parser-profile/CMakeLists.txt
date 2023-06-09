cmake_minimum_required(VERSION 3.20)

add_executable(json-parser-profile json-parser-profile.cpp)
target_link_libraries(json-parser-profile json-parser)
target_compile_definitions(json-parser-profile
	PRIVATE
		TRACY_ENABLE=1 
		PROFILE_CASE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/profile.json"
)
