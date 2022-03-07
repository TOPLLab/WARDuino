set(SOURCE_FILES Memory/mem.cpp Utils/util.cpp Utils/util_arduino.cpp Interpreter/interrupt_operations.cpp Debug/debug.cpp WARDuino/WARDuino.cpp Primitives/primitives.cpp Interpreter/instructions.cpp Utils/glue.cpp Utils/glue.h)

idf_component_register(SRCS "CLI/main.cpp" ${SOURCE_FILES} INCLUDE_DIRS "")

add_definitions(-DINFO=0)
add_definitions(-DDEBUG=0)
add_definitions(-DTRACE=1)
add_definitions(-DWARN=0)
