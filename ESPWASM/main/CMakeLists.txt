set(SOURCE_FILES Memory/mem.cpp Utils/util.cpp Utils/util_arduino.cpp Debug/debugger.cpp Utils/macros.cpp WARDuino/WARDuino.cpp Primitives/primitives.cpp Interpreter/instructions.cpp)

idf_component_register(SRCS "CLI/main.cpp" ${SOURCE_FILES} INCLUDE_DIRS "")

add_definitions(-DINFO=0)
add_definitions(-DDEBUG=0)
add_definitions(-DTRACE=1)
add_definitions(-DWARN=0)
