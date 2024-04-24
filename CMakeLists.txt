# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.15)
option(BUILD_ESP "Build for esp" OFF)
option(BUILD_EMULATOR "Build for emulator" OFF)
option(BUILD_UNITTEST "Build for testing" OFF)

# Use C++17
set(CMAKE_CXX_STANDARD 17)

if (ESP_PLATFORM)
    set(BUILD_ESP ON)
endif ()

if (NOT BUILD_ESP AND NOT BUILD_EMULATOR AND NOT BUILD_UNITTEST)
    message(WARNING "No build target set. Nothing to do.")
endif ()

# Build WARDuino with the ESP-IDF toolchain
if (BUILD_ESP)
    if (NOT EXISTS $ENV{IDF_PATH}/tools/cmake/project.cmake)
        message(FATAL_ERROR "Can't find $IDF_PATH/tools/cmake/project.cmake. Make sure ESP-IDF is installed and $IDF_PATH is set.")
    endif ()

    message(VERBOSE "Using ESP-IDF toolchain")

    set(EXTRA_COMPONENT_DIRS "platforms/ESP-IDF")

    include($ENV{IDF_PATH}/tools/cmake/project.cmake "${PROJECT_BINARY_DIR}/../include")
endif (BUILD_ESP)

project(WARDuino VERSION 0.4.4)

set(WARDUINO_VERSION_STRING "${PROJECT_VERSION}")
configure_file(src/config.h.in include/warduino/config.h)

# Build the emulator version of WARDuino
if (BUILD_EMULATOR)
    set(EXTERNAL_LIB_HEADERS lib/json/single_include)

    find_package(Threads REQUIRED)

    set(SOURCE_FILES
            src/WARDuino/WARDuino.cpp
            src/WARDuino/CallbackHandler.cpp
            src/Primitives/emulated.cpp
            src/Interpreter/instructions.cpp
            src/Interpreter/interpreter.cpp
            src/Memory/mem.cpp
            src/Utils/util.cpp
            src/Utils/util_arduino.cpp
            src/Utils/macros.cpp
            src/Utils/sockets.cpp
            src/Debug/debugger.cpp
            src/Edward/proxy.cpp
            src/Edward/proxy_supervisor.cpp
            src/Edward/RFC.cpp
    )

    add_definitions(-DINFO=0)
    add_definitions(-DDEBUG=0)
    add_definitions(-DTRACE=0)
    add_definitions(-DWARN=0)

    # Set default compile flags for GCC
    if (CMAKE_COMPILER_IS_GNUCXX)
        add_compile_options(-g -v -Wall -Wextra -Wunused -O3)
    endif (CMAKE_COMPILER_IS_GNUCXX)

    # WARDuino CLI
    add_executable(wdcli platforms/CLI-Emulator/main.cpp ${SOURCE_FILES})
    target_link_libraries(wdcli PRIVATE Threads::Threads)
    target_include_directories(wdcli PRIVATE ${EXTERNAL_LIB_HEADERS} "${PROJECT_BINARY_DIR}/include")
endif (BUILD_EMULATOR)

if (BUILD_UNITTEST)
    enable_testing()

    message(VERBOSE "Building for unit tests ")

    set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # for tooling support
    set(EXTERNAL_LIB_HEADERS lib/json/single_include)

    set(SOURCE_FILES
            src/WARDuino/WARDuino.cpp
            src/WARDuino/CallbackHandler.cpp
            src/Primitives/emulated.cpp
            src/Interpreter/instructions.cpp
            src/Interpreter/interpreter.cpp
            src/Memory/mem.cpp
            src/Utils/util.cpp
            src/Utils/util_arduino.cpp
            src/Utils/macros.cpp
            src/Utils/sockets.cpp
            src/Debug/debugger.cpp
            src/Edward/proxy.cpp
            src/Edward/proxy_supervisor.cpp
            src/Edward/RFC.cpp
    )

    # Set default compile flags for GCC
    if (CMAKE_COMPILER_IS_GNUCXX)
        add_compile_options(-g -v -Wall -Wextra -Wunused -O3)
    endif (CMAKE_COMPILER_IS_GNUCXX)


    include(FetchContent)
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG release-1.12.1
    )

    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    set(PATH_TO_UNIT_TEST ${PROJECT_SOURCE_DIR}/tests/unit)

    aux_source_directory(${PATH_TO_UNIT_TEST}/shared/ SHARED_SRC)

    message(DEBUG "Shared source code for unit tests path: " ${SHARED_SRC})

    file(GLOB TEST_SRC_FILES ${PATH_TO_UNIT_TEST}/*.cpp)

    message(DEBUG "Unit test source codes: " ${TEST_SRC_FILES})

    foreach (TEST_FILE ${TEST_SRC_FILES})
        get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
        message(DEBUG "Add executable for " ${TEST_FILE})
        add_executable(${TEST_NAME} ${TEST_FILE} ${SOURCE_FILES} ${SHARED_SRC})
        target_link_libraries(${TEST_NAME} gtest_main)
        target_include_directories(${TEST_NAME} PRIVATE ${EXTERNAL_LIB_HEADERS} "${PROJECT_BINARY_DIR}/include")
        add_test(${TEST_NAME} ${TEST_NAME})
    endforeach ()
endif (BUILD_UNITTEST)
