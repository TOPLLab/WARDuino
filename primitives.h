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

#include "WARDuinoDefs.h"

bool resolve_primitive(char *symbol, Primitive *val);
void install_primitives();

#endif
