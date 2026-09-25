#include "fft/battle.h"
#include "psx/types.h"

/*
 * After `rider` moves, re-registers its map position and, when it is the
 * rider of a mount (mount state MOUNT), copies tile, destination, real and
 * screen position to the mount 10 units / 0xa000 lower, then finishes the
 * acting unit's move. The two position blocks are copied through local block
 * types because the target uses struct copies (four lw/sw, and lwl/lwr for
 * the halfword-aligned screen block) that per-field stores do not reproduce.
 */

void battle_unit_copy_rider_data_to_mount(battle_unit_misc_data_t* unit, battle_unit_misc_data_t* rider) {
    battle_unit_misc_data_t* mount;

    if (rider->battle_data != 0) {
        battle_unit_set_tile_position(rider->battle_data->misc_unit_id, rider->map_x, rider->map_y, rider->map_z,
            (u8)((s16)rider->facing / 1024));
        if (rider->mount_state == BATTLE_MISC_MOUNT_STATE_MOUNT) {
            mount = battle_unit_get_misc_data_by_misc_id(rider->mount_partner_misc_id);
            if (mount != 0) {
                mount->map_x = mount->movement.bytes.destination_x = rider->map_x;
                mount->map_y = mount->movement.bytes.destination_y = rider->map_y;
                mount->map_z = mount->movement.bytes.destination_z = rider->map_z;
                mount->real = rider->real;
                mount->screen = rider->screen;
                mount->screen.vy -= 10;
                mount->real.vy -= 0xa000;
                battle_unit_set_tile_position(mount->battle_data->misc_unit_id, rider->map_x, rider->map_y,
                    rider->map_z, (u8)((s16)rider->facing / 1024));
            }
        }
    }
    unit->ability_ct_resolved |= 1;
    battle_action_set_only_movement_taken(unit->battle_data->misc_unit_id);
    if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED)
        && !(unit->movement_flags & BATTLE_EFFECTIVE_MOVEMENT_TELEPORT)) {
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
    }
    battle_gfx_update_unit_palettes();
    battle_move_init_post_movement_display();
}
