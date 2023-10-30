add_library(plugin_loader plugin_loader.cpp)
target_include_directories(plugin_loader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# define RBTHS_RELEASE_BUILD if we are in release mode
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(plugin_loader PRIVATE RBTHS_RELEASE_BUILD=1)
endif()

target_link_libraries(plugin_loader PUBLIC log_grabber PRIVATE Boost::filesystem Boost::dll dl)