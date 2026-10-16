#ifndef BITS_H
#define BITS_H

/* Bit-stream readers and writers shared by the menu overlays and WORLD. */

#include "psx/types.h"

/* readers and writers */
u32 bits_read_primary(s32 count);
void bits_write_primary(s32 value);
s32 bits_read_secondary(s32 count);
void bits_init_primary_reader(const u8* data);
void bits_init_secondary_reader(const u8* data);

/* stream state */
extern u8 g_bits_primary_reset;
extern s32 g_bits_reader_1_index;
extern u8* g_bits_primary_cursor;
extern s32 g_bits_writer_1_index;
extern u8 g_bits_reader_2_reset;
extern s32 g_bits_reader_2_index;
extern u8* g_bits_reader_2_stream;

#endif
