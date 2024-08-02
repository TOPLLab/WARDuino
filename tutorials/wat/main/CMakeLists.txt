set(SOURCE_FILES
        ../../../src/Debug/debugger.cpp
        ../../../src/Interpreter/instructions.cpp
        ../../../src/Memory/mem.cpp
        ../../../src/Primitives/idf.cpp
        ../../../src/Edward/proxy.cpp
        ../../../src/Edward/proxy_supervisor.cpp
        ../../../src/Edward/RFC.cpp
        ../../../src/Utils/macros.cpp
        ../../../src/Utils/sockets.cpp
        ../../../src/Utils/util.cpp
        ../../../src/Utils/util_arduino.cpp
        ../../../src/WARDuino/CallbackHandler.cpp
        ../../../src/WARDuino/WARDuino.cpp
        )

idf_component_register(SRCS "main.cpp" ${SOURCE_FILES} INCLUDE_DIRS ../../../lib/json/single_include/ REQUIRES driver)

add_definitions(-DESP=1)

