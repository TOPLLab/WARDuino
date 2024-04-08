# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(blinky)

#add_custom_command(
#    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/upload.h
#    COMMAND xxd -i ${CMAKE_CURRENT_SOURCE_DIR}/src/upload.wasm > ${CMAKE_CURRENT_SOURCE_DIR}/src/upload.h
#    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/upload.wasm
#)

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
    ../../src/Primitives/zephyr.cpp
    ../../src/Memory/mem.cpp
    ../../src/Utils/util.cpp
    ../../src/Utils/util_arduino.cpp
    ../../src/Utils/macros.cpp
    ../../src/Utils/sockets.cpp
    ../../src/Debug/debugger.cpp
    ../../src/Edward/proxy.cpp
    ../../src/Edward/RFC.cpp
    upload.h
)

include_directories(../../lib/json/single_include/)