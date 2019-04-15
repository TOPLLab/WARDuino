/**
 * This file lists the primitives of the language and stores them in the
 * primitives
 * This file should be included, not its (nonexistent) header file.
 */
#include "primitives.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_PRIMITIVES 2

primitive_entry primitives[NUM_PRIMITIVES] = {

    {"blink",
     [](Module* m) -> void {
       size_t cnt = m->stack[m->sp].value.uint32;
       for (size_t i = 1; i < cnt; i++) {
         printf("BLINK %u/%u!\n", i, cnt);
       }
       m->sp--;
     }},
    {"flash",
     [](Module* m) -> void {
       size_t cnt = m->stack[m->sp].value.uint32;
       for (size_t i = cnt; i > 0; i--) {
         printf("FLASH %u/%u!\n", i, cnt);
       }
       m->sp--;
     }},
};

bool resolvePrim(char* symbol, Primitive* val) {
  for (size_t i = 0; i < NUM_PRIMITIVES; i++) {
    if (!strcmp(symbol, primitives[i].name)) {
      *val = primitives[i].f;
      return true;
    }
  }
  return false;
}