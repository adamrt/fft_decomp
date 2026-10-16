#ifndef MESSAGE_BOX_H
#define MESSAGE_BOX_H

/* WORLD's message box with the typewriter text: its glyph and pen state, the three glyph staging slots, and the thread
 * that draws it. */

#include "fft/gfx.h"
#include "fft/text.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

extern glyph_blit_t g_message_box_state;
extern text_glyph_state_t g_message_box_glyph;
extern s32 g_message_box_glyph_delay;
extern RECT g_message_box_vram_rect;
extern s32 g_message_box_slot_threads[3];
extern s32 g_message_box_slot_glyph_counters[3];
extern u8 g_message_box_slot_carry_offset[4];
extern u8 g_message_box_slot_prev_width[4];
extern u8 g_message_box_glyph_images[3][8][84];
extern u8 g_message_box_previous_images[3][84];
extern u8 g_message_box_column_scratch[14];
extern s32 g_message_box_digit_divisors[];

void message_box_thread(void);
void message_box_show_with_sound(s32 message_id, s32 sound_id);

#endif
