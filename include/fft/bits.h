#ifndef BITS_H
#define BITS_H

/* Bit streams and bit arrays: the primary and secondary streams the menu overlays and WORLD share (the primary is read
 * and written most significant bit first), BUNIT's unit-list reader and WLDCORE's bit-array helpers. */

#include "psx/types.h"

/* primary and secondary streams */
extern u8 g_bits_primary_reset;
extern u8* g_bits_primary_cursor;
extern u8 g_bits_secondary_reset;
extern const u8* g_bits_secondary_cursor;

/* WORLD builds its own bodies of these two. */
void bits_init_primary_reader(u8* data);
void bits_init_secondary_reader(const u8* data);

/* The first read or write after a reset starts at bit 7 of the current byte. */
u32 bits_read_primary(s32 count);
void bits_write_primary(s32 value);
s32 bits_read_secondary(s32 count);

/* BUNIT unit list reader */
void bits_init_reader(const u8* data);
u32 bits_read(s32 bit_count);

/* WLDCORE bit arrays */
void bits_set_value(u32* bits, s32 index, s32 set);
s32 bits_test(u32* bits, s32 index);

#endif
