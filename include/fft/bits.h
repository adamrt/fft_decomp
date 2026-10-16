#ifndef BITS_H
#define BITS_H

/* Bit streams shared by the menu overlays and WORLD: a primary stream read and written most significant bit
 * first, and a second reader. */

#include "psx/libgpu.h"
#include "psx/types.h"

/* The first read or write after a reset starts at bit 7 of the current byte. */
u32 bits_read_primary(s32 count);

void bits_write_primary(s32 value);
s32 bits_read_secondary(s32 count);

/* WORLD builds its own bodies of these two. */
void bits_init_primary_reader(const u8* data);

void bits_init_secondary_reader(const u8* data);

/* The menu library's copy of bits_init_secondary_reader, which only EQUIP and WORLD define. */
void menu_bits_init_secondary_reader(const u8* data);

extern u8 g_bits_primary_reset;
extern s32 g_bits_reader_1_index;
extern u8* g_bits_primary_cursor;
extern s32 g_bits_writer_1_index;
extern u8 g_bits_reader_2_reset;
extern s32 g_bits_reader_2_index;
extern u8* g_bits_reader_2_stream;
void bits_init_reader(const u8* data);
u32 bits_read(s32 bit_count);
void bits_set_value(u32* bits, s32 index, s32 set);
s32 bits_test(u32* bits, s32 index);

#endif
