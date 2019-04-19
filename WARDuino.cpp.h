#pragma once
#include "WARDuino.h"

class WARDuino
{
	public: 
         int run_module(uint8_t *bytes, int size);
         Module* load_module(uint8_t *bytes, uint32_t byte_count, Options options);
         bool invoke(Module *m, uint32_t fidx);
         uint32_t get_export_fidx(Module *m, char *name);
};