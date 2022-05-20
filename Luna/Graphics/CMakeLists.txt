include(FetchContent)
include(FindVulkan)

FetchContent_Declare(glslang
	GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
	GIT_TAG master)
FetchContent_Declare(spirv-cross
	GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
	GIT_TAG master)
FetchContent_Declare(vma
	GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
	GIT_TAG v3.0.0)

set(ENABLE_GLSLANG_BINARIES Off CACHE BOOL "" FORCE)
set(ENABLE_HLSL Off CACHE BOOL "" FORCE)
set(SKIP_GLSLANG_INSTALL On CACHE BOOL "" FORCE)
set(SPIRV_CROSS_STATIC On CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SHARED Off CACHE BOOL "" FORCE)
set(SPIRV_CROSS_CLI Off CACHE BOOL "" FORCE)
set(VMA_DYNAMIC_VULKAN_FUNCTIONS On CACHE BOOL "" FORCE)
set(VMA_STATIC_VULKAN_FUNCTIONS Off CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glslang spirv-cross vma)

add_library(Luna-Graphics OBJECT)
target_compile_definitions(Luna-Graphics
	PUBLIC $<$<CONFIG:DEBUG>:LUNA_DEBUG>
	PRIVATE VK_ENABLE_BETA_EXTENSIONS VK_NO_PROTOTYPES VULKAN_HPP_DISPATCH_LOADER_DYNAMIC)
target_include_directories(Luna-Graphics PUBLIC Include)
target_link_libraries(Luna-Graphics
	PUBLIC Luna-Platform Luna-Utility Vulkan::Headers
	PRIVATE glfw glslang SPIRV spirv-cross-cpp VulkanMemoryAllocator)

add_subdirectory(Source)
