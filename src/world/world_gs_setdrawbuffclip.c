#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Program the active draw buffer's clip rectangle: the shared clip rectangle
 * shifted by that buffer's offsets.
 *
 * Load-bearing shapes:
 *  - `s32 x` / `s32 y` locals: the target loads clip.x/clip.y with `lh`. Read
 *    inline, the sign is dead (the sum is truncated by the s16 store) and gcc
 *    narrows both to `lhu`. Routing them through s32 locals forces the
 *    sign-extending load.
 *  - `DRAWENV* env` used for the clip.x store and the call: the target keeps
 *    &g_world_gs_drawenv in a0 and stores x at 0(a0), leaving the jal delay slot a
 *    nop. Without it gcc bases a0 at clip.w, stores x at -4(a0) and spends the
 *    delay slot on `addiu a0,a0,-4`. w/h/y stay absolute $at stores, so only
 *    this one field goes through the pointer.
 *  - The two hard-register pins work together. The target loads i, w and x
 *    before the prologue; x lives in v1 and w in a0 (a0 is then overwritten by
 *    the env pointer at +0x4c). Unpinned, w takes v1 itself, so x cannot load
 *    early and only reuses v1 at +0x44 once the w store kills it. Pinning w to
 *    $4 frees v1 early, so both loads move before the prologue. */
void world_gs_setdrawbuffclip(void) {
    DRAWENV* env = &g_world_gs_drawenv;
    s32 i = g_world_gs_active_buffer;
    register u16 w __asm__("$4") = g_world_gs_clip_rect.w;
    register s32 x __asm__("$3") = g_world_gs_clip_rect.x;
    s32 y = g_world_gs_clip_rect.y;
    s32 dx = g_world_gs_buffer_x[i];
    s32 dy = g_world_gs_buffer_y[i];

    g_world_gs_drawenv.clip.w = w;
    g_world_gs_drawenv.clip.h = g_world_gs_clip_rect.h;
    env->clip.x = x + dx;
    g_world_gs_drawenv.clip.y = y + dy;
    PutDrawEnv(env);
}
