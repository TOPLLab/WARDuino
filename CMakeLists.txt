# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.15)

#include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
include(pico_sdk_import.cmake)
include(pico_extras_import_optional.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.3.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

project(WARDuino C CXX ASM)#VERSION 0.2.3)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

add_definitions(-DINFO=0)
add_definitions(-DDEBUG=0)
add_definitions(-DTRACE=0)
add_definitions(-DWARN=0)
add_definitions(-DPICO=1)

set(WARDUINO_VERSION_STRING "${PROJECT_VERSION}")
configure_file(src/config.h.in include/warduino/config.h)

set(EXTERNAL_LIB_HEADERS lib/json/single_include)

# find_package(Threads REQUIRED)

set(SOURCE_FILES
        src/Primitives/pico.cpp
        src/WARDuino/WARDuino.cpp
        src/WARDuino/CallbackHandler.cpp
        src/Interpreter/instructions.cpp
        src/Memory/mem.cpp
        src/Utils/util.cpp
        src/Utils/util_arduino.cpp
        src/Utils/macros.cpp
        src/Utils/sockets.cpp
        src/Debug/debugger.cpp
        src/Edward/proxy.cpp
        #src/Edward/proxy_supervisor.cpp
        src/Edward/RFC.cpp)

#add_executable(wdcli main-test.c)
add_executable(warduino platforms/Pico/main.cpp ${SOURCE_FILES})
target_link_libraries(warduino pico_stdlib pico_multicore)
target_include_directories(warduino PRIVATE ${EXTERNAL_LIB_HEADERS} "${PROJECT_BINARY_DIR}/include")

pico_enable_stdio_usb(warduino 1)
pico_enable_stdio_uart(warduino 0)
#pico_enable_stdio_uart(warduino 1)
pico_add_extra_outputs(warduino)

add_custom_target(flash COMMAND sudo picotool load -v -x warduino.uf2 -f)
add_custom_target(compile COMMAND xxd -i upload.wasm > ${CMAKE_SOURCE_DIR}/platforms/Pico/upload.h)

