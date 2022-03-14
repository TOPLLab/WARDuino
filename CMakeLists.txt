# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
option(BUILD_ESP "Build for esp" ON)
option(BUILD_EMULATOR "Build for emulator" OFF)

# Build WARDuino with the ESP-IDF toolchain
if (BUILD_ESP)
    if (NOT EXISTS $ENV{IDF_PATH}/tools/cmake/project.cmake)
        message(FATAL_ERROR "Can't find $IDF_PATH/tools/cmake/project.cmake. Make sure ESP-IDF is installed and $IDF_PATH is set.")
    endif ()
    
    set(EXTRA_COMPONENT_DIRS "platforms")

    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
endif (BUILD_ESP)

project(hello_world)

# Build the emulator version of WARDuino
if (BUILD_EMULATOR)
    find_package(Threads REQUIRED)
    set(SOURCE_FILES
            src/Memory/mem.cpp
            src/Utils/util.cpp
            src/Utils/util_arduino.cpp
            src/Debug/debugger.cpp
            src/Utils/macros.cpp
            src/WARDuino/WARDuino.cpp
            src/WARDuino/CallbackHandler.cpp
            src/Primitives/primitives.cpp
            src/Interpreter/instructions.cpp
            )

    add_compile_options(-g -v -std=c++11 -Wall -Wextra -Wunused)
    add_executable(wdcli platforms/CLI-Emulator/main.cpp ${SOURCE_FILES})
    target_link_libraries(wdcli PRIVATE Threads::Threads)
    add_definitions(-DINFO=0)
    add_definitions(-DDEBUG=0)
    add_definitions(-DTRACE=1)
    add_definitions(-DWARN=0)
endif (BUILD_EMULATOR)
