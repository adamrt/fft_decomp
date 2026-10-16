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
 * one of 4. world_map_capture_backdrop indexes the GsOT arrays with their 0x14-byte stride. */
extern GsOT_TAG g_world_map_ot_tags[2][16];
extern GsOT g_world_map_ordering_tables[2];
extern GsOT_TAG g_world_map_aux_ot_tags[2][4];
extern GsOT g_world_map_aux_ordering_tables[2];
extern PACKET g_world_map_primitive_buffers[];
/* g_system_flags as saved by world_map_toggle_captured_frame (system_flags_e). */
extern u32 g_world_map_saved_system_flags;
void world_map_draw_frame(void);
void world_map_set_display_rect(s32 buffer_index, RECT* rect);
void world_map_clear_vram_and_scratch(s32 clear_scratch);
void world_map_clear_screen_and_set_clip_rect(s32 mode);
void world_map_draw_tiles(GsOT* ot);
void world_map_capture_backdrop(void);
void world_map_toggle_captured_frame(s32 capture);

/* screen fade */
/* g_world_map_fade.flags[]: 32-bit flags[0] controls the screen fade and its fill
 * box; flags[1] controls a second fill box. world_map_fade_start takes OUT,
 * REDRAW_MAP, STOP_MUSIC and START_MUSIC and adds RUNNING. */
typedef enum world_map_fade_flag {
    WORLD_MAP_FADE_RUNNING = 0x01,
    WORLD_MAP_FADE_OUT = 0x02,         /* fades to black; stays set while the screen is dark */
    WORLD_MAP_FADE_REDRAW_MAP = 0x04,  /* every fade step sets SYSTEM_FLAG_DRAW_MAP */
    WORLD_MAP_FADE_BOX_VISIBLE = 0x08, /* the fill box is drawn without a running fade */
    WORLD_MAP_FADE_STOP_MUSIC = 0x10,  /* also fades the music out and unloads it */
    WORLD_MAP_FADE_START_MUSIC = 0x20, /* also loads AUDIO_MUSIC(1, MUSIC_WORLD_MAP), fading it in */
} world_map_fade_flag_e;

/* The screen fade and its two full-screen fill boxes. The parallel arrays share one object, which is why the
 * target builds all three walk pointers from a single symbol address. */
typedef struct world_map_fade {
    u32 flags[2];      /* 0x00; world_map_fade_flag_e */
    s32 priorities[2]; /* 0x08; GsSortBoxFill OT priority of each box, read as a 16-bit value */
    s32 elapsed;       /* 0x10; fade frames run, stepped by world_map_fade_step */
    s32 duration;      /* 0x14 */
    GsBOXF boxes[2];   /* 0x18; full-screen fill boxes drawn by world_map_fade_draw_boxes */
} world_map_fade_t;
typedef char wldcore_sortbox_state_size_must_be_56[sizeof(world_map_fade_t) == 56 ? 1 : -1];

extern world_map_fade_t g_world_map_fade;
/* Per-channel tint factors, one byte each, 0x80 == unity. */
extern rgb24_t g_world_map_brightness_rgb;
void world_map_fade_start(s32 flags, s32 duration);
s32 world_map_fade_step(void);
void world_map_fade_draw_boxes(void);
void world_map_fade_tint_color(CVECTOR* source, CVECTOR* out);

/* value and date displays */
/* g_hud_value_mode: the layout of the HUD's value display
 * (hud_draw_value). */
typedef enum hud_value_mode {
    HUD_VALUE_PLAIN = 0,          /* eight digits */
    HUD_VALUE_FUNDS = 1,          /* labeled and underlined; the war funds */
    HUD_VALUE_PLAY_TIME = 2,      /* hours, minutes and seconds */
    HUD_VALUE_LABELED = 3,        /* PLAIN under a small label */
    HUD_VALUE_COST_AND_FUNDS = 4, /* a labeled five-digit secondary value, then FUNDS */
} hud_value_mode_e;

extern s32 g_hud_value_mode;
/* Screen position of the context-value display; the sound-novel menu saves
 * and restores both 32-bit values as one point32_t. */
extern point32_t g_hud_value_position;
/* Saved copy of g_hud_value_position. */
extern point32_t g_hud_saved_value_position;
/* The values the display shows: the main value, a secondary one, and the seconds of a play time. */
extern s32 g_hud_value;
extern s32 g_hud_value_secondary;
extern s32 g_hud_value_tertiary;
/* Color of the context-value display, tinted by world_map_fade_tint_color. */
extern CVECTOR g_hud_color;
/* HUD ordering-table priority. The sort calls read it as a 16-bit value (lhu);
 * its three setters store a whole 32-bit value. */
extern s32 g_hud_ot_priority;
void hud_draw_value(GsOT* ot);
void calendar_draw_date(GsOT* ot);

/* pictures and backgrounds */
enum { WLDCORE_BACKDROP_WORDS = 256 * 240 / 2 };

/* Sector offsets of each picture set in WLDPIC.BIN and each background set in WLDBK.BIN. */
extern s32 g_picture_sector_offsets[];
extern s32 g_picture_background_sector_offsets[];
/* Destination image buffer (read back as a TIM by sound_novel_opcode_0a_load_picture). */
extern u32* g_picture_buffer;
/* Begins with the 256x240 16-bit backdrop image, WLDCORE_BACKDROP_WORDS 32-bit words. */
extern u32* g_world_map_scratch_buffer;
/* Currently resident picture set, reset to -1 by world_map_init_state. */
extern s32 g_picture_loaded_set;
extern s32 g_picture_background_loaded_set;
/* Offsets of the 1 KiB bands of a picture, in the order its dissolving upload fills them. */
extern u16* g_picture_dissolve_band_order;
void picture_load_background_set(s32 set);
s32 location_load_picture(s32 index);
void picture_load_set(s32 value);

#endif
