set(SOURCE_FILES
        ../../../src/Memory/mem.cpp
        ../../../src/Utils/util.cpp
        ../../../src/Utils/util_arduino.cpp
        ../../../src/Debug/debugger.cpp
        ../../../src/Utils/macros.cpp
        ../../../src/WARDuino/WARDuino.cpp
        ../../../src/Primitives/emulated.cpp
        ../../../src/Interpreter/instructions.cpp
        ../../../src/WARDuino/CallbackHandler.cpp
        )

idf_component_register(SRCS "main.cpp" ${SOURCE_FILES} INCLUDE_DIRS "" REQUIRES driver)

add_definitions(-DESP=1)

