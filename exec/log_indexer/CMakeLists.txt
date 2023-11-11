
find_package(Threads)
include(FindProtobuf)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
add_executable(logindexer_server main.cpp service_impl.cpp)
target_include_directories(logindexer_server
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(logindexer_server
    PRIVATE
    logindexer_proto
    gRPC::grpc
    gRPC::grpc++
    gRPC::grpc++_reflection
    ${_REFLECTION}
    absl::flags
    absl::flags_parse
    log_grabbers
    plugin_loader
    utils
    stdc++fs
    Boost::uuid
)