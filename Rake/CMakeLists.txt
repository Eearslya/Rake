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
target_include_directories(Rake PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(Rake PRIVATE Luna SPSCQueue stb TracyClient)

target_sources(Rake PRIVATE
	AABB.cpp
	BVHNode.cpp
	Camera.cpp
	CheckerTexture.cpp
	HittableList.cpp
	ImageTexture.cpp
	Main.cpp
	Plane.cpp
	Rake.cpp
	Rectangle.cpp
	SolidTexture.cpp
	Sphere.cpp
	Tracer.cpp)
add_subdirectory(Materials)

add_custom_target(Run
	COMMAND Rake
	DEPENDS Rake
	WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
