include(FetchContent)

FetchContent_Declare(SPSCQueue
	GIT_REPOSITORY https://github.com/rigtorp/SPSCQueue.git
	GIT_TAG master)
FetchContent_Declare(tracy
	GIT_REPOSITORY https://github.com/wolfpld/tracy.git
	GIT_TAG master)

FetchContent_MakeAvailable(SPSCQueue tracy)

add_executable(Rake)
target_compile_definitions(Rake PRIVATE TRACY_ENABLE)
target_link_libraries(Rake PRIVATE Luna SPSCQueue stb TracyClient)

target_sources(Rake PRIVATE
	Camera.cpp
	HittableList.cpp
	Main.cpp
	Rake.cpp
	Sphere.cpp
	Tracer.cpp)

add_custom_target(Run
	COMMAND Rake
	DEPENDS Rake
	WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
