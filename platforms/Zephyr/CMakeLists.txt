cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(warduino)

# Note on _POSIX_C_SOURCE: If you define this macro to a value greater than or equal to 200809L, then the functionality from the 2008 edition of the POSIX standard (IEEE Standard 1003.1-2008) is made available.
target_compile_definitions(app PRIVATE _POSIX_C_SOURCE=200809L)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/upload.h
    COMMAND xxd -i upload.wasm > upload.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/upload.wasm
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(app PRIVATE 
    main.cpp
    ../../src/WARDuino/WARDuino.cpp
    ../../src/WARDuino/CallbackHandler.cpp
    ../../src/Interpreter/instructions.cpp
    ../../src/Interpreter/interpreter.cpp
    ../../src/Primitives/zephyr.cpp
    ../../src/Primitives/Mindstorms/Motor.cpp
    ../../src/Primitives/Mindstorms/uart_sensor.cpp
    ../../src/Memory/mem.cpp
    ../../src/Utils/util.cpp
    ../../src/Utils/util_arduino.cpp
    ../../src/Utils/macros.cpp
    ../../src/Utils/sockets.cpp
    ../../src/Debug/debugger.cpp
    ../../src/Edward/proxy.cpp
    ../../src/Edward/proxy_supervisor.cpp
    ../../src/Edward/RFC.cpp
    upload.h
)

include_directories(../../lib/json/single_include/)
