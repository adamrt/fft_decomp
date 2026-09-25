#include "fft/battle.h"
#include "psx/types.h"

/* Advances a jumping unit's ascent. Once its vertical velocity is no longer
 * negative, the unit takes the destination level and, when step flag 0x10 is
 * set, records the unit standing on the destination tile (id plus one). */
void battle_move_update_airborne_ascent_phase(battle_unit_misc_data_t* unit) {
    battle_unit_misc_data_t* occupant;
    s32 direction;

    battle_gfx_load_trap_and_unit_frame_parts(
        unit, (battle_unit_anim_state_t*)&unit->sprite_graphic_trigger, unit->animation_frame, unit->encoded_animation);
    direction = battle_move_get_direction(unit);
    if (g_battle_game_state != BATTLE_GAME_STATE_EVENT) {
        battle_move_displace_overlapping_unit(unit, direction);
    }
    if (unit->velocity.vy >= 0) {
        unit->centre_tile_offset = g_battle_move_ascent_centre_offsets[direction];
        unit->map_z = unit->movement.bytes.destination_z;
        if ((g_battle_move_step_value & 0x10) != 0) {
            occupant = battle_unit_find_any_misc_data_at_map_coords(unit->movement.bytes.destination_x,
                unit->movement.bytes.destination_y, unit->movement.bytes.destination_z);
            g_battle_move_tile_occupant = occupant;
            if (occupant != 0) {
                unit->current_unit_id_plus_one = occupant->battle_data->misc_unit_id + 1;
                return;
            }
        }
        unit->current_unit_id_plus_one = 0;
    }
}
