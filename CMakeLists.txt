cmake_minimum_required(VERSION 3.10)
project(pb-c-test C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PROTOBUF_C_ROOT "${CMAKE_SOURCE_DIR}/protobuf-c")

include_directories(
    ${PROTOBUF_C_ROOT}/include
    ${CMAKE_SOURCE_DIR}
)

link_directories(${PROTOBUF_C_ROOT}/lib)

add_executable(pb-server
    pb-server.c
    proto/kv.pb-c.c
    libstorage.c
)
target_link_libraries(pb-server
    protobuf-c
)
set_target_properties(pb-server PROPERTIES
    BUILD_RPATH "${PROTOBUF_C_ROOT}/lib"
    INSTALL_RPATH "${PROTOBUF_C_ROOT}/lib"
)

add_executable(pb-client
    pb-client.c
    proto/kv.pb-c.c
)
target_link_libraries(pb-client
    protobuf-c
)
set_target_properties(pb-client PROPERTIES
    BUILD_RPATH "${PROTOBUF_C_ROOT}/lib"
    INSTALL_RPATH "${PROTOBUF_C_ROOT}/lib"
)

add_executable(ascii-server
    ascii-server.c
    libstorage.c
)
add_executable(ascii-client
    ascii-client.c
)
