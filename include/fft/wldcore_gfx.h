#ifndef WLDCORE_GFX_H
#define WLDCORE_GFX_H

/* WLDCORE's frame drawing: the ordering tables, the captured world frame, screen fades, the value and date displays,
 * and the pictures and backgrounds it loads. */

#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* frame */
/* The libgs ordering tables of the two display buffers and their tags: the main table of 16 tags and an auxiliary
 * one of 4. wldcore_gfx_capture_map_backdrop indexes the GsOT arrays with their 0x14-byte stride. */
extern GsOT_TAG g_wldcore_gfx_ot_tags[2][16];
extern GsOT g_wldcore_gfx_ordering_tables[2];
extern GsOT_TAG g_wldcore_gfx_aux_ot_tags[2][4];
extern GsOT g_wldcore_gfx_aux_ordering_tables[2];
extern PACKET g_wldcore_gfx_world_primitive_buffers[];
/* g_main_system_flags as saved by wldcore_gfx_toggle_captured_world_frame (main_system_flags_e). */
extern u32 g_wldcore_saved_system_flags;
void wldcore_gfx_draw_world_frame(void);
void wldcore_gfx_set_display_rect(s32 buffer_index, RECT* rect);
void wldcore_gfx_clear_vram_and_scratch(s32 clear_scratch);
void wldcore_clear_screen_and_set_map_clip_rect(s32 mode);
void wldcore_gfx_draw_projected_map_tiles(GsOT* ot);
void wldcore_gfx_capture_map_backdrop(void);
void wldcore_gfx_toggle_captured_world_frame(s32 capture);

/* screen fade */
/* g_wldcore_screen_fade_state.flags[]: 32-bit flags[0] controls the screen fade and its fill
 * box; flags[1] controls a second fill box. wldcore_fade_start_screen takes OUT,
 * REDRAW_MAP, STOP_MUSIC and START_MUSIC and adds RUNNING. */
typedef enum wldcore_screen_fade_flag {
    WLDCORE_SCREEN_FADE_RUNNING = 0x01,
    WLDCORE_SCREEN_FADE_OUT = 0x02,         /* fades to black; stays set while the screen is dark */
    WLDCORE_SCREEN_FADE_REDRAW_MAP = 0x04,  /* every fade step sets MAIN_SYSTEM_FLAG_DRAW_MAP */
    WLDCORE_SCREEN_FADE_BOX_VISIBLE = 0x08, /* the fill box is drawn without a running fade */
    WLDCORE_SCREEN_FADE_STOP_MUSIC = 0x10,  /* also fades the music out and unloads it */
    WLDCORE_SCREEN_FADE_START_MUSIC = 0x20, /* also loads WLDCORE_AUDIO_MUSIC(1, MAIN_MUSIC_WORLD_MAP), fading it in */
} wldcore_screen_fade_flag_e;

/* The screen fade and its two full-screen fill boxes. The parallel arrays share one object, which is why the
 * target builds all three walk pointers from a single symbol address. */
typedef struct wldcore_sortbox_state {
    u32 flags[2];      /* 0x00; wldcore_screen_fade_flag_e */
    s32 priorities[2]; /* 0x08; GsSortBoxFill OT priority of each box, read as a 16-bit value */
    s32 elapsed;       /* 0x10; fade frames run, stepped by wldcore_fade_step_screen */
    s32 duration;      /* 0x14 */
    GsBOXF boxes[2];   /* 0x18; full-screen fill boxes drawn by wldcore_fade_draw_screen_overlays */
} wldcore_sortbox_state_t;
typedef char wldcore_sortbox_state_size_must_be_56[sizeof(wldcore_sortbox_state_t) == 56 ? 1 : -1];

extern wldcore_sortbox_state_t g_wldcore_screen_fade_state;
/* Per-channel tint factors, one byte each, 0x80 == unity. */
extern rgb24_t g_wldcore_screen_brightness_rgb;
void wldcore_fade_start_screen(s32 flags, s32 duration);
s32 wldcore_fade_step_screen(void);
void wldcore_fade_draw_screen_overlays(void);
void wldcore_gfx_copy_color_with_tint(CVECTOR* source, CVECTOR* out);

/* value and date displays */
/* g_wldcore_context_value_display_mode: the layout of the HUD's value display
 * (wldcore_gfx_draw_context_value_display). */
typedef enum wldcore_value_display_mode {
    WLDCORE_VALUE_DISPLAY_PLAIN = 0,          /* eight digits */
    WLDCORE_VALUE_DISPLAY_FUNDS = 1,          /* labeled and underlined; the war funds */
    WLDCORE_VALUE_DISPLAY_PLAY_TIME = 2,      /* hours, minutes and seconds */
    WLDCORE_VALUE_DISPLAY_LABELED = 3,        /* PLAIN under a small label */
    WLDCORE_VALUE_DISPLAY_COST_AND_FUNDS = 4, /* a labeled five-digit secondary value, then FUNDS */
} wldcore_value_display_mode_e;

extern s32 g_wldcore_context_value_display_mode;
/* Screen position of the context-value display; the sound-novel menu saves
 * and restores both 32-bit values as one point32_t. */
extern point32_t g_wldcore_context_value_display_position;
/* Saved copy of g_wldcore_context_value_display_position. */
extern point32_t g_wldcore_saved_context_value_display_position;
/* The values the display shows: the main value, a secondary one, and the seconds of a play time. */
extern s32 g_wldcore_displayed_numeric_value;
extern s32 g_wldcore_displayed_numeric_value_secondary;
extern s32 g_wldcore_displayed_numeric_value_tertiary;
/* Color of the context-value display, tinted by wldcore_gfx_copy_color_with_tint. */
extern CVECTOR g_wldcore_hud_color;
/* HUD ordering-table priority. The sort calls read it as a 16-bit value (lhu);
 * its three setters store a whole 32-bit value. */
extern s32 g_wldcore_hud_ot_priority;
void wldcore_gfx_draw_context_value_display(GsOT* ot);
void wldcore_gfx_draw_calendar_date(GsOT* ot);

/* pictures and backgrounds */
enum { WLDCORE_BACKDROP_WORDS = 256 * 240 / 2 };

/* Sector offsets of each picture set in WLDPIC.BIN and each background set in WLDBK.BIN. */
extern s32 g_wldcore_picture_sector_offsets[];
extern s32 g_wldcore_background_sector_offsets[];
/* Destination image buffer (read back as a TIM by wldcore_sound_novel_opcode_0a_load_picture). */
extern u32* g_wldcore_picture_buffer;
/* Begins with the 256x240 16-bit backdrop image, WLDCORE_BACKDROP_WORDS 32-bit words. */
extern u32* g_wldcore_scratch_buffer;
/* Currently resident picture set, reset to -1 by wldcore_init_world_map_state. */
extern s32 g_wldcore_loaded_picture_set;
extern s32 g_wldcore_loaded_background_set;
/* Offsets of the 1 KiB bands of a picture, in the order its dissolving upload fills them. */
extern u16* g_wldcore_dissolve_band_order;
void wldcore_gfx_load_background_set(s32 set);

#endif
