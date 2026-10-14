/*
 * BATTLE twin of WORLD script_run_sprite_move (identical code except the
 * checked-division guard, which this default-profile copy lacks).
 *
 * Sprite-move worker called by script_sprite_move (mode 0) and
 * script_sprite_move_beta (mode 1) with the script cursor: a 16-bit
 * unit id, three destination 16-bit values, an easing byte, a weight byte and a
 * 16-bit value that is a frame count (mode 0) or a speed (mode 1). pos[0] is the
 * start, pos[1] the destination, pos[2] the current position and pos[3] the
 * negated previous step, re-applied each frame so the unit's coordinate is
 * rewritten rather than accumulated. The unit id is published as the current
 * thread's 32-bit task value 1, which script_waitspritemove_event_instruction
 * polls.
 */
#include "fft/battle.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* The parameters are copied into locals in this order: it is what places the
 * s0 copy before the s2 copy in the prologue. */
void script_run_sprite_move(const event_sprite_move_parameters_t* instruction, s32 use_speed) {
    s32 pos[4][4];
    SVECTOR vec;
    const u8* parameters;
    s32 mode;
    SVECTOR* coords;
    s32 runtime_id;
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

    parameters = instruction->unit_id_le;
    mode = use_speed;
    runtime_id = script_resolve_runtime_id(script_read_s16(parameters));
    if (runtime_id == EVENT_RUNTIME_ID_NONE) {
        return;
    }
    g_thread_contexts[g_thread_current_id].task_words[0] = runtime_id;
    coords = battle_unit_get_event_offset_by_runtime_id(runtime_id);
    /* One advancing operand cursor preserves the retail register allocation. */
    parameters = instruction->destination_le[0];
    pos[0][0] = coords->vx;
    pos[0][1] = coords->vy;
    pos[0][2] = coords->vz;
    pos[3][0] = -coords->vx;
    pos[3][1] = -coords->vy;
    pos[3][2] = -coords->vz;
    for (i = 0; i < 3; i++) {
        pos[1][i] = script_read_s16(parameters);
        parameters += sizeof(instruction->destination_le[i]);
    }
    curve = *parameters++;
    weight = *parameters++;
    total = script_read_s16(parameters);
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
        thread_yield();
        for (j = 0; j < 3; j++) {
            delta = pos[1][j] - pos[0][j];
            if (curve == 0) {
                pos[2][j] = battle_mul_div_s64(delta, i, total) + pos[0][j];
            } else if (curve == 1) {
                weighted = delta * (16 - weight);
                scaled = (delta * weight) * 2;
                step = battle_mul_div_s64(scaled + weighted + weighted, i, total * 32);
                step += battle_mul_div_s64(scaled, (total - i) * i, (total * 32) * total);
                pos[2][j] = step + pos[0][j];
            } else if (curve == 3) {
                weighted = delta * (16 - weight);
                step = battle_mul_div_s64((delta * weight) * 2, i, total);
                step = battle_mul_div_s64(step + weighted + weighted, i, total * 32);
                pos[2][j] = step + pos[0][j];
            } else if (curve == 2) {
                mid = total / 2;
                half = delta + ((u32)delta >> 31);
                if (mid == 0) {
                    mid = 1;
                }
                if (i * 2 < total) {
                    weighted = delta * (16 - weight);
                    step = battle_mul_div_s64((delta * weight) * 2, i, mid);
                    step = battle_mul_div_s64(step + weighted + weighted, i, mid * 64);
                    pos[2][j] = step + pos[0][j];
                } else {
                    weighted = delta * (16 - weight);
                    scaled = (delta * weight) * 2;
                    elapsed = i - mid;
                    step = battle_mul_div_s64(scaled, mid - elapsed, mid);
                    step = battle_mul_div_s64(scaled + step + weighted + weighted, elapsed, mid * 64);
                    /* Extra use: lifts half's allocation priority above j and total (s6/s7/fp). */
                    __asm__("" : : "r"(half));
                    pos[2][j] = step + pos[0][j] + (half >> 1);
                }
            }
        }
        if (battle_unit_has_runtime_id(runtime_id) == 0) {
            thread_exit_current();
        }
        setVector(&vec, pos[3][0], pos[3][1], pos[3][2]);
        battle_unit_add_event_offset_by_runtime_id(runtime_id, &vec);
        setVector(&vec, pos[2][0] / 256, pos[2][1] / 256, pos[2][2] / 256);
        battle_unit_add_event_offset_by_runtime_id(runtime_id, &vec);
        pos[3][0] = -vec.vx;
        pos[3][1] = -vec.vy;
        pos[3][2] = -vec.vz;
    }
    if (battle_unit_has_runtime_id(runtime_id) == 0) {
        thread_exit_current();
    }
    setVector(&vec, pos[3][0], pos[3][1], pos[3][2]);
    battle_unit_add_event_offset_by_runtime_id(runtime_id, &vec);
    setVector(&vec, pos[1][0] / 256, pos[1][1] / 256, pos[1][2] / 256);
    battle_unit_add_event_offset_by_runtime_id(runtime_id, &vec);
    if (total == 1) {
        thread_yield();
    }
}
