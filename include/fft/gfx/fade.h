#ifndef FADE_H
#define FADE_H

/* Whole-screen fades: BATTLE's fade polygons, the event library's fade overlay, the menu library's screen_ fades,
 * OPEN's overlay fade, and the battle screen's color tint (fade_tint_). */

#include "psx/libgpu.h"
#include "psx/types.h"

/* fade overlay */
/* Full-screen fade primitive pair: a TILE followed by the DR_MODE that
 * selects its texture page and window. */
typedef struct fade_event_tile {
    TILE tile;    /* 0x00 */
    DR_MODE mode; /* 0x10 */
} fade_event_tile_t;
typedef char fade_event_tile_size_must_be_28[(sizeof(fade_event_tile_t) == 28) ? 1 : -1];

/* screen fade */
extern s32 g_fade_battle_step;
extern s16 g_fade_battle_level;
extern POLY_F4 g_fade_battle_polygons[2];
extern DR_MODE g_fade_battle_draw_modes[];

/* BATTLE and WORLD each define two overlays, one per packet buffer. */
extern fade_event_tile_t g_fade_event_tiles[2];

/* event screen fade */
/* Gray level the screen-covering fade tile subtracts: 0 is clear, 255 is black. */
extern s32 g_fade_event_level;

void fade_event_submit_tile(fade_event_tile_t* overlay, s32 level);
void fade_event_run_out(void);
void fade_event_run_in(void);

/* menu fades */
/* Separate decreasing/increasing RGB-intensity flags.
 * The getter packs them into bits 0/1; WORLD's returns 0 while its fades are disabled. */
typedef enum fade_menu_state {
    FADE_MENU_STATE_NONE = 0,
    FADE_MENU_STATE_DECREASING = 1,
    FADE_MENU_STATE_INCREASING = 2
} fade_menu_state_e;

/* The menu library's fades, which BUNIT and CARD run; WORLD has its own bodies. */
void fade_menu_start_out(void);
s32 fade_menu_update_out(void);
void fade_menu_start_in(void);
s32 fade_menu_update_in(void);
s32 fade_menu_get_state(void);
void fade_menu_set_disabled(u8 disabled);

/* OPEN overlay fade */
/* fade_tile_t.flags. */
typedef enum fade_tile_flag {
    FADE_TILE_OUT = 0x01,   /* the overlay clears */
    FADE_TILE_IN = 0x02,    /* the overlay darkens the screen */
    FADE_TILE_ACTIVE = 0x04 /* the overlay is drawn; cleared when either fade ends */
} fade_tile_flag_e;

/* The screen overlay fade: one record, since fade_tile_update
 * reaches flags..rect and the tiles/modes from a single base. */
typedef struct fade_tile {
    /* 0x00 */ u32 flags; /* fade_tile_flag_e */
    /* 0x04 */ s32 frame;
    /* 0x08 */ s32 duration;
    /* 0x0c */ s32 level;
    /* 0x10 */ s32 abr;
    /* 0x14 */ s32 ot_index;
    /* 0x18 */ RECT rect;
    /* 0x20 */ TILE tiles[2];
    /* 0x40 */ DR_MODE modes[2];
} fade_tile_t;

extern fade_tile_t g_fade_tile;

void fade_tile_start_in(u32 duration);
void fade_tile_start_out(u32 duration);
void fade_tile_update(u32* otag);

/* color tint */
/* Overlay draw-mode primitives, one per screen polarity. */
extern DR_MODE g_fade_tint_draw_modes[2];
extern POLY_F4 g_fade_tint_polygons[2];

void fade_tint_start(u16 blend_mode, u16 red, u16 green, u16 blue, u16 duration);
void fade_tint_update(void);
void fade_tint_draw(void);

#endif
