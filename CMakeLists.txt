# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
option(BUILD_ESP "Build for esp" OFF)
option(BUILD_EMULATOR "Build for emulator" OFF)

project(WARDuino)

if (NOT BUILD_ESP AND NOT BUILD_EMULATOR)
    message(WARNING "No build target set. Nothing to do.")
endif ()


# Build WARDuino with the ESP-IDF toolchain
if (BUILD_ESP)
    if (NOT EXISTS $ENV{IDF_PATH}/tools/cmake/project.cmake)
        message(FATAL_ERROR "Can't find $IDF_PATH/tools/cmake/project.cmake. Make sure ESP-IDF is installed and $IDF_PATH is set.")
    endif ()
    
    set(EXTRA_COMPONENT_DIRS "platforms/ESP-IDF")

    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
endif (BUILD_ESP)

# Build the emulator version of WARDuino
if (BUILD_EMULATOR)
    cmake_minimum_required(VERSION 3.14)

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
    set(TEST_FRAMEWORK
        tests/integration/wasm_tests.cpp
        tests/integration/assertion.cpp
        tests/integration/sexpr-parser/src/sexpr.c
        )

    add_definitions(-DINFO=1)
    add_definitions(-DDEBUG=0)
    add_definitions(-DTRACE=0)
    add_definitions(-DWARN=0)

    # Use C++11
    set(CMAKE_CXX_STANDARD 11)

    # Set default compile flags for GCC
    if(CMAKE_COMPILER_IS_GNUCXX)
        add_compile_options(-g -v -Wall -Wextra -Wunused)
    endif(CMAKE_COMPILER_IS_GNUCXX)

    # WARDuino CLI
    add_executable(wdcli platforms/CLI-Emulator/main.cpp ${SOURCE_FILES} ${TEST_FRAMEWORK})
    target_link_libraries(wdcli PRIVATE Threads::Threads)
endif (BUILD_EMULATOR)
