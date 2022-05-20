include(FetchContent)

FetchContent_Declare(tracy
	GIT_REPOSITORY https://github.com/wolfpld/tracy.git
	GIT_TAG master)

FetchContent_MakeAvailable(tracy)

add_executable(Rake)
target_compile_definitions(Rake PRIVATE TRACY_ENABLE)
target_link_libraries(Rake PRIVATE Luna stb TracyClient)

target_sources(Rake PRIVATE
	Main.cpp
	Rake.cpp)

add_custom_target(Run
	COMMAND Rake
	DEPENDS Rake
	WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
