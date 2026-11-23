/* Full-screen subtractive fades shared by the title menus, prologue and ending. */

#include "fft/gfx/fade.h"
#include "fft/gfx/gfx.h"
#include "fft/sys/system.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Reveal the screen over `duration` frames: the subtractive overlay fades out. */
void fade_tile_start_out(u32 duration) {
    u32 flags = g_fade_tile.flags;

    g_fade_tile.duration = duration;
    g_fade_tile.frame = 0;
    g_fade_tile.flags = flags | (FADE_TILE_OUT | FADE_TILE_ACTIVE);
}

/* Fade the screen to black over `duration` frames: the subtractive overlay fades
 * in. */
void fade_tile_start_in(u32 duration) {
    u32 flags = g_fade_tile.flags;

    g_fade_tile.duration = duration;
    g_fade_tile.frame = 0;
    g_fade_tile.flags = flags | (FADE_TILE_IN | FADE_TILE_ACTIVE);
}

/* Advance the screen overlay fade and queue its full-screen tile.
 *
 * A fade-out lowers the subtractive level from 0xff to 0 and clears
 * SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED on its second frame; a fade-in raises
 * the level to 0xff and sets the flag on its last frame. While the fade is active
 * the TILE and its DR_MODE go into ordering-table entry ot_index.
 *
 * With a single struct, cse keeps the flags address in a register (the target's
 * `la a3`/`la a2`/`la s1`) and the stores through tiles[] serialize the x/y/w/h
 * 16-bit copies exactly as the target does.  Per-block locals give block 2 its
 * own a2/a1/a0/v1 registers. */
void fade_tile_update(u32* ot) {
    if (g_fade_tile.flags & FADE_TILE_OUT) {
        s32 duration = g_fade_tile.duration;
        s32 frame = g_fade_tile.frame;
        g_fade_tile.level = 0xff - (0x100 / duration) * frame;
        if (g_fade_tile.level < 0) {
            g_fade_tile.level = 0;
        }
        if (frame == 1) {
            g_system_runtime_flags &= ~SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
        }
        g_fade_tile.frame = frame + 1;
        if (duration < frame + 1) {
            g_fade_tile.flags &= ~(FADE_TILE_OUT | FADE_TILE_ACTIVE);
        }
    }

    if (g_fade_tile.flags & FADE_TILE_IN) {
        s32 duration = g_fade_tile.duration;
        s32 frame = g_fade_tile.frame;
        g_fade_tile.level = (0x100 / duration) * frame;
        if (g_fade_tile.level >= 0x100) {
            g_fade_tile.level = 0xff;
        }
        if (frame == duration - 1) {
            g_system_runtime_flags |= SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
        }
        g_fade_tile.frame = frame + 1;
        if (duration < frame + 1) {
            g_fade_tile.flags &= ~(FADE_TILE_IN | FADE_TILE_ACTIVE);
        }
    }

    if (g_fade_tile.flags & FADE_TILE_ACTIVE) {
        TILE* tiles = g_fade_tile.tiles;
        SetTile(&tiles[g_gfx_active_buffer_index]);
        SetSemiTrans(&tiles[g_gfx_active_buffer_index], 1);
        SetDrawMode(&g_fade_tile.modes[g_gfx_active_buffer_index], 1, 0, GetTPage(0, g_fade_tile.abr, 0x100, 0), 0);
        g_fade_tile.tiles[g_gfx_active_buffer_index].x0 = g_fade_tile.rect.x;
        g_fade_tile.tiles[g_gfx_active_buffer_index].y0 = g_fade_tile.rect.y;
        g_fade_tile.tiles[g_gfx_active_buffer_index].w = g_fade_tile.rect.w;
        g_fade_tile.tiles[g_gfx_active_buffer_index].h = g_fade_tile.rect.h;
        setRGB0(&tiles[g_gfx_active_buffer_index], g_fade_tile.level, g_fade_tile.level, g_fade_tile.level);
        AddPrim(&ot[g_fade_tile.ot_index], &tiles[g_gfx_active_buffer_index]);
        AddPrim(&ot[g_fade_tile.ot_index], &g_fade_tile.modes[g_gfx_active_buffer_index]);
    }
}
