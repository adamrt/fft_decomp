#include "fft/battle_action.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "psx/types.h"

/* Advances one unit's status bubble by g_main_system_animation_speed frames.
 *
 * Every 16 timer ticks the bubble id walks forward to the next status the unit
 * carries, wrapping at 0x16, and the bubble's x/y offsets are chosen from the
 * unit's SHP type and animation.
 *
 * The block-scoped bubble_mask keeps GCC from rotating the loop's leading exit
 * tests to the bottom, and indexing g_battle_gfx_status_bubble_status_masks inside the loop lets loop.c
 * strength-reduce it into the target's pointer walk. */
void battle_gfx_determine_status_bubble_parameters(battle_unit_t* unit) {
    s32 frame;
    s32 bubble;
    u32 status_flags;
    s32 animation;

    for (frame = 0; frame < g_main_system_animation_speed; frame++) {
        bubble = unit->status_bubble_id;
        if (bubble == 9) {
            unit->status_bubble_alternate_row = (unit->status_bubble_timer & 8) >> 3;
        } else {
            unit->status_bubble_alternate_row = (unit->status_bubble_timer & 0x10) >> 4;
        }

        if ((unit->status_bubble_timer & 0xF) == 0) {
            status_flags = unit->status_flags_1_4;
            if (((status_flags & BATTLE_UNIT_STATUS_BUBBLE_MASK) == 0)
                && (unit->runtime_id != g_battle_unit_source_runtime_id)) {
                unit->status_bubble_active = 0;
                return;
            }

            for (;;) {
                s32 bubble_mask;

                if (bubble == 0) {
                    if (status_flags & BATTLE_UNIT_STATUS_DEAD) {
                        break;
                    }
                } else if (bubble == 0x14) {
                    if (status_flags & BATTLE_UNIT_STATUS_DEATH_SENTENCE) {
                        break;
                    }
                }
                bubble++;
                if (bubble >= 0x16) {
                    bubble = 0;
                }
                if (bubble == unit->status_bubble_id) {
                    break;
                }
                if (bubble != 0x15) {
                    bubble_mask = g_battle_gfx_status_bubble_status_masks[bubble];
                    if (status_flags & bubble_mask) {
                        break;
                    }
                } else if (unit->runtime_id == g_battle_unit_source_runtime_id) {
                    break;
                }
            }
            unit->status_bubble_id = bubble;

            switch (g_battle_gfx_spritesheet_data[unit->spritesheet_id].shp_id) {
            case BATTLE_SHP_ARUTE:
                unit->status_bubble_x = 0;
                unit->status_bubble_y = -0x46;
                break;
            case BATTLE_SHP_KANZEN:
                unit->status_bubble_x = 0;
                unit->status_bubble_y = -0x78;
                break;
            case BATTLE_SHP_OTHER:
                animation = unit->animation.sequence_id >> 1;
                if (animation == BATTLE_UNIT_ANIMATION_DEAD || animation == BATTLE_UNIT_ANIMATION_DEATH) {
                    unit->status_bubble_x = -5;
                    unit->status_bubble_y = -0x19;
                } else {
                    unit->status_bubble_x = 0;
                    unit->status_bubble_y = -0x19;
                }
                break;
            case BATTLE_SHP_TYPE1:
            case BATTLE_SHP_TYPE2:
                if (map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z)->depth_half_height.depth) {
                    switch (unit->animation.sequence_id >> 1) {
                    case BATTLE_UNIT_ANIMATION_DEAD:
                    case BATTLE_UNIT_ANIMATION_DEATH:
                        unit->status_bubble_x = -5;
                        unit->status_bubble_y = -0x23;
                        break;
                    case BATTLE_UNIT_ANIMATION_SLEEP:
                        unit->status_bubble_x = 0;
                        unit->status_bubble_y = -0x1E;
                        break;
                    default:
                        unit->status_bubble_x = 0;
                        unit->status_bubble_y = -0x28;
                        break;
                    }
                } else {
                    switch (unit->animation.sequence_id >> 1) {
                    case BATTLE_UNIT_ANIMATION_DEAD:
                    case BATTLE_UNIT_ANIMATION_DEATH:
                        unit->status_bubble_x = -5;
                        unit->status_bubble_y = -0x19;
                        break;
                    case BATTLE_UNIT_ANIMATION_SLEEP:
                        unit->status_bubble_x = 0;
                        unit->status_bubble_y = -0x1E;
                        break;
                    default:
                        unit->status_bubble_x = 0;
                        unit->status_bubble_y = -0x28;
                        break;
                    }
                }
                break;
            default:
                animation = unit->animation.sequence_id >> 1;
                if (animation == BATTLE_UNIT_ANIMATION_DEAD || animation == BATTLE_UNIT_ANIMATION_DEATH) {
                    unit->status_bubble_x = -5;
                    unit->status_bubble_y = -0x1E;
                } else {
                    unit->status_bubble_x = 0;
                    unit->status_bubble_y = -0x32;
                }
                break;
            }
        }
        unit->status_bubble_timer++;
    }
}
