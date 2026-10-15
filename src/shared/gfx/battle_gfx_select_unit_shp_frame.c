#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"

/* Return the frame tables that a unit frame id indexes and the frame itself:
 * a battle_gfx_source_frame_t pointer.
 *
 * A frame below BATTLE_SHP_FRAME_EVTCHR_FIRST is one of the unit's own SHP.
 * Its submerged table replaces the plain one for a rider that is not
 * jumping, and for a unit on a water tile whose surface is 12 or more above
 * the unit's feet; from 36 above, the unit shows submerged frame 0 whatever
 * the frame id. A unit that floats or moves on water, a crystal and a
 * treasure keep the plain table, and a chicken or a frog changes at 4.
 * Higher frame ids index the SHP of an EVTCHR slot: slot 0 for the sequences
 * below 600 and slot 1 for the rest, the submerged table on a water tile. */
void battle_gfx_select_unit_shp_frame(battle_unit_t* unit, battle_unit_anim_state_t* state, u16 frame, u16 sequence_id,
    battle_gfx_unit_shp_frame_tables_t** out_table, battle_gfx_source_frame_t** out_frame) {
    map_tile_t* tile;
    battle_gfx_unit_shp_frame_tables_t* table;
    battle_gfx_source_frame_t* selected;
    s32 flags;
    s32 height;
    s32 z;

    tile = map_get_tile_data_pointer(
        (unit->world_coords.vx / MAP_TILE_SIZE), (unit->world_coords.vz / MAP_TILE_SIZE), unit->map_z);
    if (frame < BATTLE_SHP_FRAME_EVTCHR_FIRST) {
        table = state->shp;
        if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
            if (unit->status_flags_5_6 & (BATTLE_UNIT_STATUS_JUMP | BATTLE_UNIT_STATUS_JUMP_HEIGHT_ACTIVE)) {
                selected = table->frames[frame];
            } else {
                selected = table->submerged_frames[frame];
            }
        } else if (tile->depth_half_height.depth) {
            if (unit->stats != 0) {
                flags = move_get_effective_flags(unit->stats);
                /* The combined test keeps the table pointer's reference count
                 * low enough for the target's s0/s1 allocation. */
                if (unit->status_flags_5_6 & BATTLE_UNIT_STATUS_FLOAT) {
                    selected = table->frames[frame];
                } else if ((flags & BATTLE_EFFECTIVE_MOVEMENT_ON_WATER) || (flags & BATTLE_EFFECTIVE_MOVEMENT_FLOAT)) {
                    selected = table->frames[frame];
                } else if (unit->spritesheet_id == BATTLE_SPRITESHEET_ID_CRYSTAL) {
                    selected = table->frames[frame];
                } else if (unit->spritesheet_id == BATTLE_SPRITESHEET_ID_TREASURE) {
                    selected = table->frames[frame];
                } else if (unit->spritesheet_id == BATTLE_SPRITESHEET_ID_CHICKEN
                    || unit->spritesheet_id == BATTLE_SPRITESHEET_ID_FROG) {
                    if ((tile->height + tile->depth_half_height.depth) * 12 >= -unit->world_coords.vy + 4) {
                        selected = table->submerged_frames[frame];
                    } else {
                        selected = table->frames[frame];
                    }
                } else {
                    height = (tile->height + tile->depth_half_height.depth) * 12;
                    z = -unit->world_coords.vy;
                    if (height >= z + 0x24) {
                        selected = table->submerged_frames[0];
                    } else if (height >= z + 0xc) {
                        selected = table->submerged_frames[frame];
                    } else {
                        selected = table->frames[frame];
                    }
                }
            } else {
                height = (tile->height + tile->depth_half_height.depth) * 12;
                z = -unit->world_coords.vy;
                if (height >= z + 0x24) {
                    selected = table->submerged_frames[0];
                } else if (height >= z + 0xc) {
                    selected = table->submerged_frames[frame];
                } else {
                    selected = table->frames[frame];
                }
            }
        } else {
            selected = table->frames[frame];
        }
    } else {
        table = &g_battle_gfx_vram_slots[1].shp.tables;
        if (sequence_id < BATTLE_SEQUENCE_EVTCHR_SLOT_1) {
            table = &g_battle_gfx_vram_slots[0].shp.tables;
        }
        if (tile->depth_half_height.depth) {
            selected = table->submerged_frames[frame - BATTLE_SHP_FRAME_EVTCHR_FIRST];
        } else {
            selected = table->frames[frame - BATTLE_SHP_FRAME_EVTCHR_FIRST];
        }
    }
    *out_table = table;
    *out_frame = selected;
}
