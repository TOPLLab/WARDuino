cmake_minimum_required(VERSION 3.14)
project (WARDuino)
add_compile_options(-g -v -std=c++11  -Wall -Wextra -Wunused)
set(SOURCE_FILES  mem.cpp util.cpp util_arduino.cpp  interrupt_operations.cpp debug.cpp WARDuino.cpp primitives.cpp instructions.cpp socket_server.cpp)
add_executable(WARDuino demo/run_wa/main.cpp ${SOURCE_FILES})
# add_executable(TestWARDuino tests/main.cpp ${SOURCE_FILES} tests/assertion.cpp tests/sexpr-parser/src/sexpr.c) #cmake does not compile when enabled

#add_definitions(-DARDUINO)
add_definitions(-DDEBUG=0)
add_definitions(-DTRACE=0)
add_definitions(-DWARN=0)

#socket sever options
add_definitions(-DSOCKET=1)
set(MultiThread  1) #enable socket sever on seperate thread

if (${MultiThread})
    add_definitions(-DMT=1)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(WARDuino PRIVATE Threads::Threads)
endif()

add_executable(WARDuinoBench benchmarks/benchmarks.cpp ${SOURCE_FILES})
install (TARGETS WARDuino DESTINATION demo/run_wa/bin)
# add_custom_command (
#         TARGET WARDuinoBench
#         POST_BUILD
#         COMMAND make -C  ../benchmarks/tasks all
# )

