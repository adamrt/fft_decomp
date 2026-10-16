#ifndef FADE_H
#define FADE_H

/* Whole-screen fades and the color tint (fade_tint_) of the battle screen. */

#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

extern s32 g_fade_step;
extern rgb24_t g_fade_tint;
extern rgb24_t g_fade_tint_backup;
extern u16 g_fade_tint_fade_frames;
extern u16 g_fade_tint_step[3];
extern rgb24_t g_fade_tint_target;
extern u16 g_fade_tint_value[3];
extern fade_overlay_t g_fade_overlays[]; /* one fade overlay per packet buffer */

/* Overlay draw-mode primitives, one per screen polarity. */
extern DR_MODE g_fade_tint_draw_modes[2];

extern POLY_F4 g_fade_tint_polygons[2];
extern RECT g_fade_tint_tex_window;
extern DR_MODE g_fade_draw_modes[];
extern s16 g_fade_intensity;
extern POLY_F4 g_fade_polygons[2];
void fade_tint_draw_overlay(void);
void fade_tint_start_fade(u16 blend_mode, u16 red, u16 green, u16 blue, u16 duration);
void fade_tint_update_fade(void);

#endif
