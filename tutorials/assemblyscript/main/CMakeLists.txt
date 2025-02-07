set(SOURCE_FILES
        ../../../src/Memory/mem.cpp
        ../../../src/Utils/util.cpp
        ../../../src/Utils/util_arduino.cpp
        ../../../src/Utils/sockets.cpp
        ../../../src/Debug/debugger.cpp
        ../../../src/Oop/proxy.cpp
        ../../../src/Oop/proxy_supervisor.cpp
        ../../../src/Oop/RFC.cpp
        ../../../src/Utils/macros.cpp
        ../../../src/WARDuino/WARDuino.cpp
        ../../../src/Primitives/emulated.cpp
        ../../../src/Interpreter/instructions.cpp
        ../../../src/WARDuino/CallbackHandler.cpp
        )

idf_component_register(SRCS "main.cpp" ${SOURCE_FILES} INCLUDE_DIRS ../../../lib/json/single_include/ REQUIRES driver)

add_definitions(-DESP=1)

