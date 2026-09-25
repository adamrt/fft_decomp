#include "fft/battle.h"
#include "fft/world.h"
#include "psx/types.h"

s16 world_camera_pick_nearest_unblocked_yaw(s32 misc_id_a, s32 misc_id_b) {
    s32 yaw;
    s32 mask;
    s32 raw;
    /* The target allocates the mask bit to $7, ahead of raw/ptr/i; without
     * the binding GCC shifts all four up one slot in REG_ALLOC_ORDER. The
     * pin is safe because bit is live only after the last call. */
    register s32 bit __asm__("$7");
    s32 best;
    s32 sign;
    s32 i;
    s32 delta;
    s32 wrapped;
    s32 dist;

    yaw = world_camera_wrap_yaw_angle() & 0xFFF;
    world_unit_get_map_coords_from_misc_id(misc_id_a, g_world_text_speaker_screen_coords);
    mask = battle_map_get_tile_data_value(0xB, g_world_text_speaker_screen_coords[0],
               g_world_text_speaker_screen_coords[2], g_world_text_speaker_screen_coords[1])
        & 0xFF;
    world_unit_get_map_coords_from_misc_id(misc_id_b, g_world_text_speaker_screen_coords);
    mask = (mask
               | battle_map_get_tile_data_value(0xB, g_world_text_speaker_screen_coords[0],
                   g_world_text_speaker_screen_coords[2], g_world_text_speaker_screen_coords[1]))
        & 0xF;
    /* The retail loop tests the masked bit with a halfword sign extension
     * (sll 16); GCC folds it away once it can see the mask fits in 4 bits.
     * The tied empty asm hides the known-zero bits without emitting code. */
    __asm__("" : "=r"(mask) : "0"(mask));
    bit = 1;
    best = 0x8000;
    sign = 1;
    for (i = 0; i < 4; i++) {
        if ((s16)(bit & mask) == 0) {
            delta = g_world_camera_candidate_yaws[i] - yaw;
            raw = delta;
            if (delta > 0) {
                wrapped = delta - 0x1000;
            } else {
                wrapped = delta + 0x1000;
            }
            if (delta < 0) {
                delta = -delta;
            }
            if (wrapped < 0) {
                wrapped = -wrapped;
            }
            /* At the head of the join block: reorg would otherwise steal the
             * first join instruction into the preceding bgez delay slot and
             * retarget the branch past it. The target leaves that slot empty.
             * An asm is not a delay-slot candidate; this emits no instruction. */
            __asm__ volatile("");
            if (wrapped < delta) {
                dist = wrapped;
            } else {
                dist = delta;
            }
            if (dist < best) {
                best = dist;
                if (raw > 0) {
                    sign = -1;
                    if (delta < wrapped) {
                        sign = 1;
                    }
                } else {
                    sign = 1;
                    if (delta < wrapped) {
                        sign = -1;
                    }
                }
            }
        }
        bit <<= 1;
    }
    return (u16)g_world_event_words[EVENT_SCRIPT_VAR_CAMERA_YAW] + sign * best;
}
