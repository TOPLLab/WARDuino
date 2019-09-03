/* 
 __      __                  .___    .__               
/  \    /  \_____ _______  __| _/_ __|__| ____   ____  
\   \/\/   /\__  \\_  __ \/ __ |  |  \  |/    \ /  _ \ 
 \        /  / __ \|  | \/ /_/ |  |  /  |   |  (  <_> )
  \__/\  /  (____  /__|  \____ |____/|__|___|  /\____/ 
       \/        \/           \/             \/        
       
WARDuino (c) by Christophe Scholliers & Robbert Gurdeep Singh 

WARDuino is licensed under a
Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

You should have received a copy of the license along with this
work. If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.
*/
#ifndef WARDUINO_PRIM_H
#define WARDUINO_PRIM_H

#include "WARDuino.h"

/**
 * Find a primitive function by looking it up
 *
 * @param symbol name of primitive function
 * @param val    location to store a pointer to the function
 * @return true if function is found and assigned to val
 */
bool resolve_primitive(char *symbol, Primitive *val);

/**
 * Handle import of memory by keeping a array of 256 pages.
 * If more imports of memory are needed, this should be replaced by a
 * nicer function that allows you to specify the different kinds of
 * memory that mey be needed
 * 
 * @param symbol name
 * @param val    location to store memory to
 * @return true if memory is found and assigned to val
 */
bool resolve_external_memory(char *symbol, Memory** val);

void install_primitives();

#endif
