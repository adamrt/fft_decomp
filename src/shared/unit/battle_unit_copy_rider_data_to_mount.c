#include "fft/battle_action.h"
#include "fft/battle_gfx.h"
#include "fft/battle_move.h"
#include "fft/battle_unit.h"

/*
 * After `rider` moves, re-registers its map position and, when it is the
 * rider of a mount (mount state MOUNT), copies tile, destination, real and
 * screen position to the mount 10 units / 0xa000 lower, then finishes the
 * acting unit's move. The two position blocks are copied through local block
 * types because the target uses struct copies (four lw/sw, and lwl/lwr for
 * the 16-bit-aligned screen block) that per-field stores do not reproduce.
 */

void battle_unit_copy_rider_data_to_mount(battle_unit_t* unit, battle_unit_t* rider) {
    battle_unit_t* mount;

    if (rider->stats != 0) {
        battle_unit_set_tile_position(
            rider->stats->battle_id, rider->map_x, rider->map_y, rider->map_z, (rider->facing / 1024));
        if (rider->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
            mount = battle_unit_get_by_runtime_id(rider->mount_partner_runtime_id);
            if (mount != 0) {
                mount->map_x = mount->movement.destination_x = rider->map_x;
                mount->map_y = mount->movement.destination_y = rider->map_y;
                mount->map_z = mount->movement.destination_z = rider->map_z;
                mount->real = rider->real;
                mount->world_coords = rider->world_coords;
                mount->world_coords.vy -= 10;
                mount->real.vy -= 0xa000;
                battle_unit_set_tile_position(
                    mount->stats->battle_id, rider->map_x, rider->map_y, rider->map_z, (rider->facing / 1024));
            }
        }
    }
    unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_MOVED;
    action_set_only_movement_taken(unit->stats->battle_id);
    if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED)
        && !(unit->path.flags & BATTLE_EFFECTIVE_MOVEMENT_TELEPORT)) {
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
    }
    battle_gfx_update_unit_palettes();
    move_init_post_movement_display();
}
