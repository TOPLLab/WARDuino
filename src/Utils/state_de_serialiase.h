#pragma once

#include "../WARDuino.h"

/**
 * Deserialise a global value according the binary protocol and put results in
 * dest
 *
 * @param dest
 * @param bytes
 * @return True if successful
 */
bool deserialiseStackValueIntoDest(StackValue *dest, uint8_t *bytes);
