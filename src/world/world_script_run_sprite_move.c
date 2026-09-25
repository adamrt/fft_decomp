/*
 * Sprite-move worker called by world_script_sprite_move (mode 0) and
 * world_script_sprite_move_beta (mode 1) with the script cursor: a unit
 * halfword, three destination halfwords, an easing byte, a weight byte and a
 * halfword that is a frame count (mode 0) or a speed (mode 1). pos[0] is the
 * start, pos[1] the destination, pos[2] the current position and pos[3] the
 * negated previous step, re-applied each frame so the unit's coordinate is
 * rewritten rather than accumulated. Rows are 16 bytes apart in the target.
 *
 * Built with the divcheck profile, which reproduces the checked-division
 * guard after SquareRoot0.
 */
#include "fft/battle.h"
#include "fft/event.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gte.h"
#include "psx/types.h"

/* The parameters are copied into locals in this order: it is what places the
 * s0 copy before the s2 copy in the prologue. */
void world_script_run_sprite_move(void* arg, s32 use_speed) {
    s32 pos[4][4];
    battle_screen_coords_t vec;
    u8* parameters;
    s32 mode;
    s16* coords;
    s32 misc_id;
    s32 weight;
    s32 curve;
    s32 total;
    s32 i;
    s32 j;
    s32 delta;
    s32 weighted;
    s32 scaled;
    s32 step;
    s32 mid;
    s32 elapsed;
    s32 half;

    parameters = (u8*)arg;
    mode = use_speed;
    misc_id = world_get_misc_id(world_script_load_halfword(parameters));
    if (misc_id == EVENT_MISC_ID_NONE) {
        return;
    }
    g_world_thread_contexts[g_world_thread_current_id].task_words[0] = misc_id;
    coords = battle_unit_get_event_offset_ptr_by_misc_id(misc_id);
    parameters += 2;
    pos[0][0] = coords[0];
    pos[0][1] = coords[1];
    pos[0][2] = coords[2];
    pos[3][0] = -coords[0];
    pos[3][1] = -coords[1];
    pos[3][2] = -coords[2];
    for (i = 0; i < 3; i++) {
        pos[1][i] = world_script_load_halfword(parameters);
        parameters += 2;
    }
    curve = *parameters++;
    weight = *parameters++;
    total = world_script_load_halfword(parameters);
    if (mode == 1) {
        for (j = 0; j < 3; j++) {
            pos[2][j] = ((pos[1][j] - pos[0][j]) * (pos[1][j] - pos[0][j])) << 4;
        }
        total = SquareRoot0(pos[2][0] + pos[2][1] + pos[2][2]) / total;
    }
    for (i = 0; i < 3; i++) {
        pos[0][i] <<= 8;
        pos[1][i] <<= 8;
    }
    for (i = 1; i < total; i++) {
        world_thread_yield();
        for (j = 0; j < 3; j++) {
            delta = pos[1][j] - pos[0][j];
            if (curve == 0) {
                pos[2][j] = world_mul_div_64(delta, i, total) + pos[0][j];
            } else if (curve == 1) {
                weighted = delta * (16 - weight);
                scaled = (delta * weight) * 2;
                step = world_mul_div_64(scaled + weighted + weighted, i, total * 32);
                step += world_mul_div_64(scaled, (total - i) * i, (total * 32) * total);
                pos[2][j] = step + pos[0][j];
            } else if (curve == 3) {
                weighted = delta * (16 - weight);
                step = world_mul_div_64((delta * weight) * 2, i, total);
                step = world_mul_div_64(step + weighted + weighted, i, total * 32);
                pos[2][j] = step + pos[0][j];
            } else if (curve == 2) {
                mid = total / 2;
                half = delta + ((u32)delta >> 31);
                if (mid == 0) {
                    mid = 1;
                }
                if (i * 2 < total) {
                    weighted = delta * (16 - weight);
                    step = world_mul_div_64((delta * weight) * 2, i, mid);
                    step = world_mul_div_64(step + weighted + weighted, i, mid * 64);
                    pos[2][j] = step + pos[0][j];
                } else {
                    weighted = delta * (16 - weight);
                    scaled = (delta * weight) * 2;
                    elapsed = i - mid;
                    step = world_mul_div_64(scaled, mid - elapsed, mid);
                    step = world_mul_div_64(scaled + step + weighted + weighted, elapsed, mid * 64);
                    /* Use: one extra reference lifts half above j and total for the target's s6/s7/fp. */
                    __asm__("" : : "r"(half));
                    pos[2][j] = step + pos[0][j] + (half >> 1);
                }
            }
        }
        if (battle_unit_has_misc_id(misc_id) == 0) {
            world_thread_exit_current();
        }
        vec.x = pos[3][0];
        vec.z = pos[3][1];
        vec.y = pos[3][2];
        battle_unit_add_event_offset_by_misc_id(misc_id, &vec);
        vec.x = pos[2][0] / 256;
        vec.z = pos[2][1] / 256;
        vec.y = pos[2][2] / 256;
        battle_unit_add_event_offset_by_misc_id(misc_id, &vec);
        pos[3][0] = -vec.x;
        pos[3][1] = -vec.z;
        pos[3][2] = -vec.y;
    }
    if (battle_unit_has_misc_id(misc_id) == 0) {
        world_thread_exit_current();
    }
    vec.x = pos[3][0];
    vec.z = pos[3][1];
    vec.y = pos[3][2];
    battle_unit_add_event_offset_by_misc_id(misc_id, &vec);
    vec.x = pos[1][0] / 256;
    vec.z = pos[1][1] / 256;
    vec.y = pos[1][2] / 256;
    battle_unit_add_event_offset_by_misc_id(misc_id, &vec);
    if (total == 1) {
        world_thread_yield();
    }
}
