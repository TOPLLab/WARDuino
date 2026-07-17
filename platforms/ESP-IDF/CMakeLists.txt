set(SOURCE_FILES
        ../../src/Debug/debugger.cpp
        ../../src/Debug/debug_msg.cpp
        ../../src/Interpreter/instructions.cpp
        ../../src/Memory/mem.cpp
        ../../src/Primitives/idf.cpp
        ../../src/Edward/proxy.cpp
        ../../src/Edward/proxy_supervisor.cpp
        ../../src/Edward/RFC.cpp
        ../../src/Utils/macros.cpp
        ../../src/Utils/sockets.cpp
        ../../src/Utils/util.cpp
        ../../src/Utils/util_arduino.cpp
        ../../src/WARDuino/CallbackHandler.cpp
        ../../src/WARDuino/event_structs.cpp
        ../../src/WARDuino/WARDuino.cpp
        ../../src/WARDuino/vm_exception.cpp
        ../../src/Interrupts/interrupt_inspect.cpp
        ../../src/Instrumentation/instrumentation.cpp
        ../../src/Interrupts/interrupt_around_function.cpp
        ../../src/Interrupts/interrupt_remote_call.cpp
        ../../src/Interrupts/interrupt_hook_on_addr.cpp
        ../../src/Interrupts/interrupt_hook_on_event.cpp
        ../../src/Interrupts/interrupt_hook_on_error.cpp
        ../../src/Interrupts/interrupt_response.cpp
        ../../src/Instrumentation/logical_clock.cpp
        ../../src/Instrumentation/hook.cpp
)


idf_component_register(SRCS "main.cpp" ${SOURCE_FILES} INCLUDE_DIRS ../../lib/json/single_include/ REQUIRES driver)

add_definitions(-DINFO=0)
add_definitions(-DDEBUG=0)
add_definitions(-DTRACE=0)
add_definitions(-DWARN=0)
add_definitions(-DESP=1)
