#ifndef MEMORY_H
#define MEMORY_H

/* Game memory loops; each entry keeps its original argument order and bounds. */

#include "psx/types.h"

/* byte copies */
/* Copy forward; count must be positive. */
void mem_copy_bytes_nonempty(void* destination, const void* source, s32 count);

/* Copy forward using a signed destination-end address; nonpositive counts do nothing. */
void battle_mem_copy_bytes(void* destination, const void* source, s32 byte_count);

/* Source-first byte copies; nonpositive counts leave the destination unchanged. */
void main_mem_copy_bytes_source_first(const void* source, void* destination, int count);
void main_mem_copy_byte_data_source_first(const void* source, void* destination, s32 count);

/* wide copies */
/* Copy byte_count / 2 unsigned 16-bit values; leave an odd final byte unchanged. */
void battle_mem_copy_u16_values(void* destination, const void* source, s32 byte_count);

/* zeroing */
/* Clear count bytes; nonpositive counts leave the destination unchanged. */
void main_mem_zero(void* destination, s32 count);

/* Clear byte_count / 4 signed 32-bit values; leave any trailing bytes unchanged. */
void battle_mem_zero_32bit_values(void* destination, s32 byte_count);

#endif /* MEMORY_H */
