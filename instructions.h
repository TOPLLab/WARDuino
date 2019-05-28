#include "WARDuino.h"



void push_block(Module *m, Block *block, int sp);
Block *pop_block(Module *m) ;
void setup_call(Module *m, uint32_t fidx);
bool interpret(Module *m);