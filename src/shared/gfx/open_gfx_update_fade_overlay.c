#include "fft/open.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Advance the screen overlay fade and queue its full-screen tile.
 *
 * A fade-out lowers the subtractive level from 0xff to 0 and clears
 * OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED on its second frame; a fade-in raises
 * the level to 0xff and sets the flag on its last frame. While the fade is active
 * the TILE and its DR_MODE go into ordering-table entry ot_index.
 *
 * With a single struct, cse keeps the flags address in a register (the target's
 * `la a3`/`la a2`/`la s1`) and the stores through tiles[] serialize the x/y/w/h
 * 16-bit copies exactly as the target does.  Per-block locals give block 2 its
 * own a2/a1/a0/v1 registers. */
void open_gfx_update_fade_overlay(u32* ot) {
    if (g_open_gfx_overlay_fade.flags & OPEN_OVERLAY_FADE_OUT) {
        s32 duration = g_open_gfx_overlay_fade.duration;
        s32 frame = g_open_gfx_overlay_fade.frame;
        g_open_gfx_overlay_fade.level = 0xff - (0x100 / duration) * frame;
        if (g_open_gfx_overlay_fade.level < 0) {
            g_open_gfx_overlay_fade.level = 0;
        }
        if (frame == 1) {
            g_open_system_runtime_flags &= ~OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
        }
        g_open_gfx_overlay_fade.frame = frame + 1;
        if (duration < frame + 1) {
            g_open_gfx_overlay_fade.flags &= ~(OPEN_OVERLAY_FADE_OUT | OPEN_OVERLAY_FADE_ACTIVE);
        }
    }
    if (g_open_gfx_overlay_fade.flags & OPEN_OVERLAY_FADE_IN) {
        s32 duration = g_open_gfx_overlay_fade.duration;
        s32 frame = g_open_gfx_overlay_fade.frame;
        g_open_gfx_overlay_fade.level = (0x100 / duration) * frame;
        if (g_open_gfx_overlay_fade.level >= 0x100) {
            g_open_gfx_overlay_fade.level = 0xff;
        }
        if (frame == duration - 1) {
            g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
        }
        g_open_gfx_overlay_fade.frame = frame + 1;
        if (duration < frame + 1) {
            g_open_gfx_overlay_fade.flags &= ~(OPEN_OVERLAY_FADE_IN | OPEN_OVERLAY_FADE_ACTIVE);
        }
    }
    if (g_open_gfx_overlay_fade.flags & OPEN_OVERLAY_FADE_ACTIVE) {
        TILE* tiles = g_open_gfx_overlay_fade.tiles;
        SetTile(&tiles[g_active_graphics_buffer_index]);
        SetSemiTrans(&tiles[g_active_graphics_buffer_index], 1);
        SetDrawMode(&g_open_gfx_overlay_fade.modes[g_active_graphics_buffer_index], 1, 0,
            GetTPage(0, g_open_gfx_overlay_fade.abr, 0x100, 0), 0);
        g_open_gfx_overlay_fade.tiles[g_active_graphics_buffer_index].x0 = g_open_gfx_overlay_fade.rect.x;
        g_open_gfx_overlay_fade.tiles[g_active_graphics_buffer_index].y0 = g_open_gfx_overlay_fade.rect.y;
        g_open_gfx_overlay_fade.tiles[g_active_graphics_buffer_index].w = g_open_gfx_overlay_fade.rect.w;
        g_open_gfx_overlay_fade.tiles[g_active_graphics_buffer_index].h = g_open_gfx_overlay_fade.rect.h;
        setRGB0(&tiles[g_active_graphics_buffer_index], g_open_gfx_overlay_fade.level, g_open_gfx_overlay_fade.level,
            g_open_gfx_overlay_fade.level);
        AddPrim(&ot[g_open_gfx_overlay_fade.ot_index], &tiles[g_active_graphics_buffer_index]);
        AddPrim(&ot[g_open_gfx_overlay_fade.ot_index], &g_open_gfx_overlay_fade.modes[g_active_graphics_buffer_index]);
    }
}
