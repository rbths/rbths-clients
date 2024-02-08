add_library(log_grabbers STATIC
log_grabbers.cpp
log_grabbers.h
)
target_compile_options(log_grabbers PRIVATE "-O3")
target_include_directories(log_grabbers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR} log_grabber)
target_link_libraries(log_grabbers PUBLIC log_grabber utils profiler Boost::regex)