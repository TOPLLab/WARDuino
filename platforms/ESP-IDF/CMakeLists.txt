set(SOURCE_FILES
        ../../src/Debug/debugger.cpp
        ../../src/Interpreter/instructions.cpp
        ../../src/Memory/mem.cpp
        ../../src/Primitives/idf.cpp
        ../../src/Edward/proxy.cpp
        ../../src/Edward/proxy_supervisor.cpp
        ../../src/Edward/RFC.cpp
        ../../src/Utils/macros.cpp
        ../../src/Utils/sockets.cpp
        ../../src/Utils/util.cpp
        ../../src/Utils/util_arduino.cpp
        ../../src/WARDuino/CallbackHandler.cpp
        ../../src/WARDuino/WARDuino.cpp
        )

    #add_custom_command(
    #        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/debug.pb.h
    #        COMMAND protoc -I=../../src/Debug --cpp_out=${CMAKE_CURRENT_SOURCE_DIR} ../../src/Debug/debug.proto
    #        DEPENDS ../../src/Debug/debug.proto
    #        VERBATIM)

idf_component_register(
        SRCS
        "main.cpp"
        ${SOURCE_FILES}
        "proto/debug.pb.cc"
        INCLUDE_DIRS
        "."
        "proto"
        PRIV_REQUIRES protobuf nvs_flash driver
) # DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/debug.pb.h)

add_definitions(-DINFO=0)
add_definitions(-DDEBUG=0)
add_definitions(-DTRACE=0)
add_definitions(-DWARN=0)
add_definitions(-DESP=1)
