include(FetchContent)

FetchContent_Declare(glm
	GIT_REPOSITORY https://github.com/g-truc/glm.git
	GIT_TAG master)
FetchContent_Declare(spdlog
	GIT_REPOSITORY https://github.com/gabime/spdlog.git
	GIT_TAG v1.10.0)

FetchContent_MakeAvailable(glm spdlog)

add_library(Luna-Utility OBJECT)
target_compile_definitions(Luna-Utility PUBLIC $<$<CONFIG:DEBUG>:LUNA_DEBUG>)
target_include_directories(Luna-Utility PUBLIC Include)
target_link_libraries(Luna-Utility PUBLIC glm spdlog)

add_subdirectory(Source)
