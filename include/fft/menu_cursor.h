#ifndef MENU_CURSOR_H
#define MENU_CURSOR_H

/* Menu cursors: the bobbing finger cursor of menu windows, the 16x16 markers that slide with a shadow, and the
 * cursor frame that zooms in. */

#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* One cursor animation threshold and its signed screen offset. */
typedef struct menu_cursor_bob_entry {
    s8 threshold;
    s8 offset;
} menu_cursor_bob_entry_t;

typedef char menu_cursor_bob_entry_size_must_be_2[sizeof(menu_cursor_bob_entry_t) == 2 ? 1 : -1];

/* Task words of a thread that bobs a menu cursor: menu_get_cursor_bob_offset
 * reads the timer, which the thread advances by g_script_event_speed each
 * frame and resets whenever it sets or clears the active flag. */
enum {
    MENU_CURSOR_THREAD_WORD_BOB_TIMER = 0,
    MENU_CURSOR_THREAD_WORD_BOB_ACTIVE = 1, /* 2 after a press or while input is elsewhere: the active curve */
};

/* The idle and active curves: (threshold, offset) pairs ending at threshold 0; entry 0 holds the modulus. */
extern menu_cursor_bob_entry_t g_menu_cursor_bob_thresholds[];

extern menu_cursor_bob_entry_t g_menu_cursor_bob_active_thresholds[];

/* The vertical offset of the current thread's cursor this frame. */
s32 menu_get_cursor_bob_offset(s32 mode);

/* Both markers keep their state in a u16: the active flag in bit 15 and the frame counter in bits 0-6. Mode 0
 * cancels the active curve and an odd mode starts it. */
extern s8 g_menu_cursor_forward_offset_keyframes[];

extern s8 g_menu_cursor_reverse_offset_keyframes[];
extern u16 g_menu_cursor_texture_page;

/* Marker CLUTs of the normal (mode0) and alternate (mode1) palettes, which menu_set_palette_mode activates. */
extern u16 g_menu_cursor_mode0_background_clut;

extern u16 g_menu_cursor_mode0_foreground_clut;
extern u16 g_menu_cursor_mode1_background_clut;
extern u16 g_menu_cursor_mode1_foreground_clut;
void menu_update_and_draw_animated_cursor(const point16_t* anchor, u16* state, s32 mode);

/* The same marker sliding horizontally; an odd mode starts the reverse curve. */
void menu_update_and_draw_sliding_cursor(point16_t* pos, u16* state, s32 mode);

/* Twelve crop percentages, indexed by step. */
extern u16 g_menu_cursor_zoom_percentages[];

/* Scales the sprite about its center, keeping its position and texture coordinates even. */
void menu_zoom_cursor_frame(const void* unused_source, SPRT* sprite, s32 step);

#endif
