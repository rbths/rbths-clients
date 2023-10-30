
add_library(journald_iterator SHARED journald_iterator.cpp)
target_include_directories(journald_iterator PUBLIC . INTERFACE log_grabber)
target_compile_options(journald_iterator PRIVATE "-O3")

target_link_libraries(journald_iterator
    PUBLIC
    log_grabber
    PRIVATE
    systemd
    utils
    Boost::dll
)