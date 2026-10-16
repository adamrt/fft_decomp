#ifndef MEMORY_H
#define MEMORY_H

/* The event library's byte copy for BATTLE and WORLD, which the menu library and both programs also call. */

#include "psx/types.h"

/* Copy forward; count must be positive. */
void mem_copy_bytes_nonempty(void* destination, const void* source, s32 count);

#endif
