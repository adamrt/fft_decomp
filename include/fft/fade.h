#ifndef FADE_H
#define FADE_H

/* Whole-screen fades and the color tint (fade_tint_) of the battle screen. */

#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* screen fade */
extern s32 g_fade_step;
extern s16 g_fade_intensity;
extern POLY_F4 g_fade_polygons[2];
extern DR_MODE g_fade_draw_modes[];

/* BATTLE and WORLD each define two overlays, one per packet buffer. */
extern fade_overlay_t g_fade_overlays[2];

/* color tint */
extern rgb24_t g_fade_tint;
extern rgb24_t g_fade_tint_backup;
extern rgb24_t g_fade_tint_target;
extern u16 g_fade_tint_value[3];
extern u16 g_fade_tint_step[3];
extern u16 g_fade_tint_fade_frames;
extern RECT g_fade_tint_tex_window;

/* Overlay draw-mode primitives, one per screen polarity. */
extern DR_MODE g_fade_tint_draw_modes[2];
extern POLY_F4 g_fade_tint_polygons[2];

void fade_tint_start_fade(u16 blend_mode, u16 red, u16 green, u16 blue, u16 duration);
void fade_tint_update_fade(void);
void fade_tint_draw_overlay(void);

#endif
