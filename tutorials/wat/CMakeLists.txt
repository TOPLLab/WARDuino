cmake_minimum_required(VERSION 3.5)

if (NOT EXISTS $ENV{IDF_PATH}/tools/cmake/project.cmake)
    message(FATAL_ERROR
        "Cannot find global project.cmake file. Make sure ESP-IDF is installed.")
endif ()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(wat)

