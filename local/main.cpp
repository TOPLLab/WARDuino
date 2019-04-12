#include "../WARDuino.h"
#include <iostream>

int main(int argc, const char * argv[]) {
    WARDuino* w = new WARDuino();
    w->run_module();
    return 0;
}

