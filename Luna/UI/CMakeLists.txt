include(FindVulkan)

add_subdirectory(ImGui)

add_library(Luna-UI OBJECT)

target_include_directories(Luna-UI PUBLIC Include)
target_link_libraries(Luna-UI
	PUBLIC imgui
	PRIVATE glfw Luna-Graphics Luna-Platform Luna-Utility Vulkan::Headers)

add_subdirectory(Source)
