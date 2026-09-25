#include "fft/battle.h"
#include "psx/types.h"

/* The bubble offsets are signed pixel offsets; storing them through a u8
 * field makes GCC materialise positive constants (li 0xba) where the target
 * uses the sign-extended form (li -0x46). */
#define STATUS_BUBBLE_X(unit) (*(s8*)&(unit)->status_bubble_x)
#define STATUS_BUBBLE_Y(unit) (*(s8*)&(unit)->status_bubble_y)

/* Advances one unit's status bubble by g_animation_speed frames.
 *
 * Every 16 timer ticks the bubble id walks forward to the next status the unit
 * carries, wrapping at 0x16, and the bubble's x/y offsets are chosen from the
 * unit's SHP type and animation.
 *
 * The block-scoped bubble_mask keeps GCC from rotating the loop's leading exit
 * tests to the bottom, and indexing g_battle_gfx_status_bubble_status_masks inside the loop lets loop.c
 * strength-reduce it into the target's pointer walk. */
void battle_gfx_determine_status_bubble_parameters(battle_unit_misc_data_t* unit) {
    s32 frame;
    s32 bubble;
    u32 status_flags;
    s32 animation;

    for (frame = 0; frame < g_animation_speed; frame++) {
        bubble = unit->status_bubble_id;
        if (bubble == 9) {
            unit->status_bubble_alternate_row = (unit->status_bubble_timer & 8) >> 3;
        } else {
            unit->status_bubble_alternate_row = (unit->status_bubble_timer & 0x10) >> 4;
        }

        if ((unit->status_bubble_timer & 0xF) == 0) {
            status_flags = unit->status_flags_1_4;
            if (((status_flags & BATTLE_MISC_STATUS_BUBBLE_MASK) == 0) && (unit->unit_id != g_battle_casting_misc_id)) {
                unit->status_bubble_active = 0;
                return;
            }

            for (;;) {
                s32 bubble_mask;

                if (bubble == 0) {
                    if (status_flags & BATTLE_MISC_STATUS_DEAD) {
                        break;
                    }
                } else if (bubble == 0x14) {
                    if (status_flags & BATTLE_MISC_STATUS_DEATH_SENTENCE) {
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
                } else if (unit->unit_id == g_battle_casting_misc_id) {
                    break;
                }
            }
            unit->status_bubble_id = bubble;

            switch (g_battle_gfx_spritesheet_data[unit->spritesheet_id].shp_id) {
            case 6:
                STATUS_BUBBLE_X(unit) = 0;
                STATUS_BUBBLE_Y(unit) = -0x46;
                break;
            case 7:
                STATUS_BUBBLE_X(unit) = 0;
                STATUS_BUBBLE_Y(unit) = -0x78;
                break;
            case 4:
                animation = unit->encoded_animation >> 1;
                if (animation == 0x1A || animation == 0x34) {
                    STATUS_BUBBLE_X(unit) = -5;
                    STATUS_BUBBLE_Y(unit) = -0x19;
                } else {
                    STATUS_BUBBLE_X(unit) = 0;
                    STATUS_BUBBLE_Y(unit) = -0x19;
                }
                break;
            case 0:
            case 1:
                if (battle_map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z)->depth_half_height
                    & MAP_TILE_DEPTH_MASK) {
                    switch (unit->encoded_animation >> 1) {
                    case 0x1A:
                    case 0x34:
                        STATUS_BUBBLE_X(unit) = -5;
                        STATUS_BUBBLE_Y(unit) = -0x23;
                        break;
                    case 0x24:
                        STATUS_BUBBLE_X(unit) = 0;
                        STATUS_BUBBLE_Y(unit) = -0x1E;
                        break;
                    default:
                        STATUS_BUBBLE_X(unit) = 0;
                        STATUS_BUBBLE_Y(unit) = -0x28;
                        break;
                    }
                } else {
                    switch (unit->encoded_animation >> 1) {
                    case 0x1A:
                    case 0x34:
                        STATUS_BUBBLE_X(unit) = -5;
                        STATUS_BUBBLE_Y(unit) = -0x19;
                        break;
                    case 0x24:
                        STATUS_BUBBLE_X(unit) = 0;
                        STATUS_BUBBLE_Y(unit) = -0x1E;
                        break;
                    default:
                        STATUS_BUBBLE_X(unit) = 0;
                        STATUS_BUBBLE_Y(unit) = -0x28;
                        break;
                    }
                }
                break;
            default:
                animation = unit->encoded_animation >> 1;
                if (animation == 0x1A || animation == 0x34) {
                    STATUS_BUBBLE_X(unit) = -5;
                    STATUS_BUBBLE_Y(unit) = -0x1E;
                } else {
                    STATUS_BUBBLE_X(unit) = 0;
                    STATUS_BUBBLE_Y(unit) = -0x32;
                }
                break;
            }
        }
        unit->status_bubble_timer++;
    }
}
