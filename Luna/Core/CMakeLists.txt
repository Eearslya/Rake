add_library(Luna-Core OBJECT)
target_compile_definitions(Luna-Core PUBLIC $<$<CONFIG:DEBUG>:LUNA_DEBUG>)
target_include_directories(Luna-Core PUBLIC Include)
target_link_libraries(Luna-Core PUBLIC
	Luna-Utility)

add_subdirectory(Source)
