set(SOURCE_FILES
        ../../src/Memory/mem.cpp
        ../../src/Utils/util.cpp
        ../../src/Utils/util_arduino.cpp
        ../../src/Debug/debugger.cpp
        ../../src/Utils/macros.cpp
        ../../src/WARDuino/WARDuino.cpp
        ../../src/Primitives/idf.cpp
        ../../src/Interpreter/instructions.cpp
        ../../src/RFC/rfc.cpp
        ../../src/RFC/proxy_supervisor.cpp
        ../../src/RFC/SocketServer.cpp
        ../../src/WARDuino/CallbackHandler.cpp
        )

idf_component_register(SRCS "main.cpp" ${SOURCE_FILES} INCLUDE_DIRS ../../lib/json/single_include/ REQUIRES driver)

add_definitions(-DINFO=0)
add_definitions(-DDEBUG=0)
add_definitions(-DTRACE=0)
add_definitions(-DWARN=0)
add_definitions(-DESP=1)
