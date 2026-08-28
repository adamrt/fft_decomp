/* The first call's argument is staged in one `arg` variable (sum, then
 * product), which local_alloc ties to $a0 through the argument copy. The
 * height difference gets its own `delta`, and the empty asm input keeps it
 * live past the sum: while `delta` dies there, the sum ties to delta's $v0
 * instead of the target's $a0. The `apex = 24` local is required: with a
 * literal 24 the second call's argument folds to a shift sequence instead of
 * the target's li/mult. The zt-before-zf local loads give the target's
 * halfword load order. */
#include "fft/battle.h"
#include "psx/types.h"

s32 battle_move_calculate_jump_arc_velocity(
    const battle_screen_coords_t* from, const battle_screen_coords_t* to, VECTOR* out) {
    s32 rise;
    s32 peak;
    s32 frames;
    s32 apex;
    s32 zf;
    s32 zt;
    s32 delta;
    s32 rise_term;

    apex = 24;
    zt = to->z;
    zf = from->z;
    delta = zf - zt;
    rise_term = delta + apex;
    /* Keeps delta live past the sum so the sum takes $a0. */
    __asm__("" : : "r"(delta));
    rise_term = rise_term * g_battle_move_jump_gravity;
    rise = SquareRoot12(rise_term * 2);
    frames = rise / g_battle_move_jump_gravity;
    out->vy = -rise;
    peak = SquareRoot12((g_battle_move_jump_gravity * apex) * 2);
    frames = frames + peak / g_battle_move_jump_gravity;
    out->vx = ((to->x - from->x) << 12) / frames;
    out->vz = ((to->y - from->y) << 12) / frames;
    return frames;
}
