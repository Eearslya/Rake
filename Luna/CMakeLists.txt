add_subdirectory(Core)
add_subdirectory(Graphics)
add_subdirectory(Platform)
add_subdirectory(UI)
add_subdirectory(Utility)

add_library(Luna STATIC)
target_include_directories(Luna PUBLIC Include)
target_link_libraries(Luna PUBLIC
	Luna-Core
	Luna-Graphics
	Luna-Platform
	Luna-UI
	Luna-Utility)
