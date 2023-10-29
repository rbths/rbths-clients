
find_package(Threads)
include(FindProtobuf)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
add_executable(log_cli main.cpp)
target_include_directories(log_cli PUBLIC logindexer_proto)
target_link_libraries(log_cli
    PRIVATE
    logindexer_proto
    gRPC::grpc
    gRPC::grpc++
    gRPC::grpc++_reflection
    ${_REFLECTION}
    absl::flags
    absl::flags_parse
)