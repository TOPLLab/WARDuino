# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
option(BUILD_ESP "Build for esp " OFF)
option(BUILD_EMULATOR "Build for emulator" ON)

# Build WARDuino with the ESP-IDF toolchain
if (BUILD_ESP)
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
endif (BUILD_ESP)

project(hello_world)

# Build the emulator version of WARDuino
if (BUILD_EMULATOR)
    find_package(Threads REQUIRED)
    set(SOURCE_FILES
            main/Memory/mem.cpp
            main/Utils/util.cpp
            main/Utils/util_arduino.cpp
            main/Interpreter/interrupt_operations.cpp
            main/Debug/debug.cpp
            main/WARDuino/WARDuino.cpp
            main/Primitives/primitives.cpp
            main/Interpreter/instructions.cpp
            main/Utils/glue.cpp
            main/Utils/glue.h)

    add_compile_options(-g -v -std=c++11 -Wall -Wextra -Wunused)
    add_executable(wdcli main/CLI-Emulator/main.cpp ${SOURCE_FILES})
    target_link_libraries(wdcli PRIVATE Threads::Threads)
    add_definitions(-DINFO=0)
    add_definitions(-DDEBUG=0)
    add_definitions(-DTRACE=1)
    add_definitions(-DWARN=0)
endif (BUILD_EMULATOR)
