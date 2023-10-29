
add_executable(log_local_cli main.cpp)
target_link_libraries(log_local_cli
    PRIVATE
    plugin_loader
    log_grabbers
    Boost::filesystem
)