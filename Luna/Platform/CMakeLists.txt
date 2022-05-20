include(FetchContent)
include(FindVulkan)

FetchContent_Declare(glfw
	GIT_REPOSITORY https://github.com/glfw/glfw.git
	GIT_TAG 3.3.6)

set(GLFW_BUILD_DOCS Off CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES Off CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS Off CACHE BOOL "" FORCE)
set(GLFW_INSTALL Off CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

add_library(Luna-Platform OBJECT)
target_compile_definitions(Luna-Platform
	PUBLIC $<$<CONFIG:Debug>:LUNA_DEBUG>
	PRIVATE GLFW_INCLUDE_VULKAN)
target_include_directories(Luna-Platform PUBLIC Include)
target_link_libraries(Luna-Platform
	PUBLIC Luna-Utility
	PRIVATE glfw Vulkan::Headers)

add_subdirectory(Source)
