# find_package(Protobuf CONFIG REQUIRED)
# include_directories(${Protobuf_INCLUDE_DIRS})
# include_directories(${CMAKE_CURRENT_BINARY_DIR})

# # find_package(gRPC CONFIG REQUIRED)
# #set(PROTOBUF_IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR})
# #set(Protobuf_SRC_ROOT_FOLDER ${CMAKE_CURRENT_SOURCE_DIR})


# file(GLOB_RECURSE PROTOS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.proto)
# set(PROTOBUF_GENERATE_CPP_APPEND_PATH OFF)
# PROTOBUF_GENERATE_CPP(PROTO_SRC PROTO_HEADER ${PROTOS})

# ADD_LIBRARY(proto ${PROTO_HEADER} ${PROTO_SRC})
# TARGET_INCLUDE_DIRECTORIES(proto PUBLIC ${PROTOBUF_INCLUDE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
# target_link_libraries(proto PRIVATE protobuf)

include(FindProtobuf)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Threads)

#
# Protobuf/Grpc source files
#
set(PROTO_FILES
    logindexer/logindexer.proto
)

#
# Add Library target with protobuf sources
#
add_library(logindexer_proto ${PROTO_FILES})
target_link_libraries(logindexer_proto
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc
        gRPC::grpc++
)
target_include_directories(logindexer_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

#
# Compile protobuf and grpc files in logindexer_proto target to cpp
#
get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
protobuf_generate(TARGET logindexer_proto LANGUAGE cpp)
protobuf_generate(TARGET logindexer_proto LANGUAGE grpc GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}")