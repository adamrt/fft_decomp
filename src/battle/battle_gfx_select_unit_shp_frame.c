#include "fft/battle.h"

/* Select the SHP frame table and frame entry for a unit.
 *
 * Frames below 0xd2 come from the unit's own SHP; a unit standing in deep
 * enough water uses the submerged half of the table unless it floats, flies
 * over the water, or is a crystal/treasure. Higher frames come from a shared
 * table selected by the animation ID. */
void battle_gfx_select_unit_shp_frame(battle_unit_misc_data_t* unit, battle_unit_anim_state_t* sprite, u16 frame,
    u16 animation, s32** out_table, s32* out_value) {
    map_tile_t* tile;
    s32* table;
    s32 value;
    s32 flags;
    s32 height;
    s32 z;

    tile = battle_map_get_tile_data_pointer((s16)(unit->screen.vx / 28), (s16)(unit->screen.vz / 28), unit->map_z);
    if (frame < 0xd2) {
        table = sprite->shp;
        if (unit->mount_state == BATTLE_MISC_MOUNT_STATE_RIDER) {
            if (unit->status_flags_5_6 & (BATTLE_MISC_STATUS_JUMP | BATTLE_MISC_STATUS_JUMP_HEIGHT_ACTIVE)) {
                value = table[frame + 2];
            } else {
                value = table[frame + 0xd2];
            }
        } else if (tile->depth_half_height & 0xe0) {
            if (unit->battle_data != 0) {
                flags = battle_move_get_effective_flags(unit->battle_data);
                /* The combined test keeps the table pointer's reference count
                 * low enough for the target's s0/s1 allocation. */
                if (unit->status_flags_5_6 & BATTLE_MISC_STATUS_FLOAT) {
                    value = table[frame + 2];
                } else if ((flags & BATTLE_EFFECTIVE_MOVEMENT_ON_WATER) || (flags & BATTLE_EFFECTIVE_MOVEMENT_FLOAT)) {
                    value = table[frame + 2];
                } else if (unit->spritesheet_id == BATTLE_SPRITESHEET_ID_CRYSTAL) {
                    value = table[frame + 2];
                } else if (unit->spritesheet_id == BATTLE_SPRITESHEET_ID_TREASURE) {
                    value = table[frame + 2];
                } else if ((u8)(unit->spritesheet_id - BATTLE_SPRITESHEET_ID_CHICKEN) < 2) {
                    if ((tile->height + (tile->depth_half_height >> 5)) * 12 >= -unit->screen.vy + 4) {
                        value = table[frame + 0xd2];
                    } else {
                        value = table[frame + 2];
                    }
                } else {
                    height = (tile->height + (tile->depth_half_height >> 5)) * 12;
                    z = -unit->screen.vy;
                    if (height >= z + 0x24) {
                        value = table[0xd2];
                    } else if (height >= z + 0xc) {
                        value = table[frame + 0xd2];
                    } else {
                        value = table[frame + 2];
                    }
                }
            } else {
                height = (tile->height + (tile->depth_half_height >> 5)) * 12;
                z = -unit->screen.vy;
                if (height >= z + 0x24) {
                    value = table[0xd2];
                } else if (height >= z + 0xc) {
                    value = table[frame + 0xd2];
                } else {
                    value = table[frame + 2];
                }
            }
        } else {
            value = table[frame + 2];
        }
    } else {
        /* Flat word view of the second slot's SHP section: header then frames. */
        table = g_battle_gfx_vram_slots[1].shp1.header;
        if (animation < 600) {
            table -= 0x1d59;
        }
        /* A shifted byte offset keeps the target's base-first addu. */
        if (tile->depth_half_height & 0xe0) {
            value = *(s32*)((u8*)table + (frame << 2));
        } else {
            value = *(s32*)((u8*)table + (frame << 2) - 0x340);
        }
    }
    *out_table = table;
    *out_value = value;
}
