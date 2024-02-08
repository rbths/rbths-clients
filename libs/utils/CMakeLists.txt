add_library(utils INTERFACE)
target_include_directories(utils INTERFACE .)


ADD_LIBRARY(profiler STATIC Profiler.cpp Profiler.h)

target_link_libraries(profiler PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(profiler PUBLIC . /usr/local/cuda/include)