#ifndef FADE_H
#define FADE_H

/* Whole-screen fades: BATTLE's fade polygons, the event library's fade overlay, the menu library's screen_ fades,
 * OPEN's overlay fade, and the battle screen's color tint (fade_tint_). */

#include "psx/libgpu.h"
#include "psx/types.h"

/* fade overlay */
/* Full-screen fade primitive pair: a TILE followed by the DR_MODE that
 * selects its texture page and window. */
typedef struct fade_overlay {
    TILE tile;    /* 0x00 */
    DR_MODE mode; /* 0x10 */
} fade_overlay_t;
typedef char fade_overlay_size_must_be_28[(sizeof(fade_overlay_t) == 28) ? 1 : -1];

/* screen fade */
extern s32 g_fade_step;
extern s16 g_fade_intensity;
extern POLY_F4 g_fade_polygons[2];
extern DR_MODE g_fade_draw_modes[];

/* BATTLE and WORLD each define two overlays, one per packet buffer. */
extern fade_overlay_t g_fade_overlays[2];

/* event screen fade */
/* Gray level the screen-covering fade tile subtracts: 0 is clear, 255 is black. */
extern s32 g_gfx_screen_fade;

void gfx_submit_screen_fade_overlay(fade_overlay_t* overlay, s32 level);
void gfx_fade_out_screen_overlay(void);
void gfx_fade_in_screen_overlay(void);

/* menu fades */
/* Separate decreasing/increasing RGB-intensity flags.
 * The getter packs them into bits 0/1; WORLD's returns 0 while its fades are disabled. */
typedef enum screen_fade_state {
    SCREEN_FADE_STATE_NONE = 0,
    SCREEN_FADE_STATE_DECREASING = 1,
    SCREEN_FADE_STATE_INCREASING = 2
} screen_fade_state_e;

/* The menu library's fades, which BUNIT and CARD run; WORLD has its own bodies. */
void screen_start_fade_out(void);
s32 screen_update_fade_out(void);
void screen_start_fade_in(void);
s32 screen_update_fade_in(void);
s32 screen_get_fade_state(void);

/* OPEN overlay fade */
/* gfx_overlay_fade_t.flags. */
typedef enum gfx_overlay_fade_flag {
    GFX_OVERLAY_FADE_OUT = 0x01,   /* the overlay clears */
    GFX_OVERLAY_FADE_IN = 0x02,    /* the overlay darkens the screen */
    GFX_OVERLAY_FADE_ACTIVE = 0x04 /* the overlay is drawn; cleared when either fade ends */
} gfx_overlay_fade_flag_e;

/* The screen overlay fade: one record, since gfx_update_overlay_fade
 * reaches flags..rect and the tiles/modes from a single base. */
typedef struct gfx_overlay_fade {
    /* 0x00 */ u32 flags; /* gfx_overlay_fade_flag_e */
    /* 0x04 */ s32 frame;
    /* 0x08 */ s32 duration;
    /* 0x0c */ s32 level;
    /* 0x10 */ s32 abr;
    /* 0x14 */ s32 ot_index;
    /* 0x18 */ RECT rect;
    /* 0x20 */ TILE tiles[2];
    /* 0x40 */ DR_MODE modes[2];
} gfx_overlay_fade_t;

extern gfx_overlay_fade_t g_gfx_overlay_fade;

void gfx_start_overlay_fade_in(u32 duration);
void gfx_start_overlay_fade_out(u32 duration);
void gfx_update_overlay_fade(u32* otag);

/* color tint */
/* Overlay draw-mode primitives, one per screen polarity. */
extern DR_MODE g_fade_tint_draw_modes[2];
extern POLY_F4 g_fade_tint_polygons[2];

void fade_tint_start_fade(u16 blend_mode, u16 red, u16 green, u16 blue, u16 duration);
void fade_tint_update_fade(void);
void fade_tint_draw_overlay(void);

#endif
