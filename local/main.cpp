#include "../WARDuino.h"
#include <iostream>

#include "../wa_sources/hello_world.c"

int main(int argc, const char * argv[]) {
    WARDuino* w = new WARDuino();
    w->run_module(hello_world_wasm,hello_world_wasm_len);
    return 0;
}

