/**
 * This file lists the primitives of the language and stores them in the
 * primitives
 * 
 * Adding a primitive:
 *  1) Bump up the NUM_PRIMITIVES constant
 *  2) Define <primitive>_str 
 *  3) Define <primitive>_type 
 *  4) Define a function void primitive(Module* m)
 *  5) Extend the install_primitives function 
 * 
 */
#include "primitives.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_PRIMITIVES 2
// Global index for installing primitives
int prim_index = 0;
// Constant strings for the primitive names
typedef const char *str_type;

/*
    Private macros to install a primitive
*/
#define install_primitive(prim_name)        \
  {                                         \
    PrimitiveEntry* p = &primitives[prim_index++]; \
    p->name = #prim_name;             \
    p->f = &prim_name;                 \
  }

#define get_arg(m,arg) m->stack[m->sp+arg].value
#define arg0 get_arg(m,0)
#define arg1 get_arg(m,1)
#define arg2 get_arg(m,2)
#define arg3 get_arg(m,2)
#define arg4 get_arg(m,2)


// The primitive table
PrimitiveEntry primitives[NUM_PRIMITIVES];

//------------------------------------------------------
// Primitive Blink
//------------------------------------------------------
Type blink_type;

void blink(Module *m)
{
  size_t cnt = arg0.uint32;
  for (size_t i = 1; i < cnt; i++)
  {
    printf("BLINK %u/%u!\n", i, cnt);
  }
  m->sp--;
}

//------------------------------------------------------
// Primitive Flash
//------------------------------------------------------
Type flash_type;

void flash(Module *m)
{
  size_t cnt = arg0.uint32;
  for (size_t i = cnt; i > 0; i--)
  {
    printf("FLASH %u/%u!\n", i, cnt);
  }
  m->sp--;
}

//------------------------------------------------------
// Installing all the primitives
//------------------------------------------------------
void install_primitives(void)
{
  install_primitive(blink);
  install_primitive(flash);
}

//------------------------------------------------------
// resolving the primitives
//------------------------------------------------------
bool resolve_primitive(char *symbol, Primitive *val)
{
  for (size_t i = 0; i < NUM_PRIMITIVES; i++)
  {
    if (!strcmp(symbol, primitives[i].name))
    {
      *val = primitives[i].f;
      return true;
    }
  }
  return false;
}