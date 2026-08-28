#include "fft/battle.h"
#include "fft/battle_state.h"

/* Moves a unit toward its action target over distortion_timer frames.
 *
 * Phase 0 picks the destination and splits the remaining distance into equal
 * Q12 steps: during an event the unit's own map tile, else the first entry of
 * the target list, else the recorded action target panel.  Phase 1 applies the
 * steps (height only when allow_height_change is set). */
void battle_unit_move_toward_action_target(battle_unit_misc_data_t* unit, s32 allow_height_change) {
    SVECTOR tile_center;
    VECTOR velocity;
    s32 timer;
    s32 target_z;
    battle_unit_misc_data_t* target;

    timer = unit->distortion_timer;
    target_z = unit->distortion_target;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        if (unit->battle_data != 0) {
            if (g_battle_game_state == BATTLE_GAME_STATE_EVENT) {
                velocity.vx = ((unit->map_x * 0x1C + 0xE) << 12) - unit->real.vx;
                velocity.vz = ((unit->map_y * 0x1C + 0xE) << 12) - unit->real.vz;
                velocity.vy = 0;
            } else if (unit->target_count != 0) {
                target = battle_unit_get_misc_data_by_battle_id(unit->target_list[0]);
                velocity.vx = target->real.vx - unit->real.vx;
                velocity.vy = target->real.vy - unit->real.vy;
                velocity.vz = target->real.vz - unit->real.vz;
            } else {
                velocity.vx = (unit->battle_data->action_target_x * 0x1C + 0xE - unit->screen.vx) << 12;
                velocity.vz = (unit->battle_data->action_target_y * 0x1C + 0xE - unit->screen.vz) << 12;
                tile_center.vx = unit->battle_data->action_target_x * 0x1C + 0xE;
                tile_center.vz = unit->battle_data->action_target_y * 0x1C + 0xE;
                /* The target loads the elevation as an s16 halfword; the u8 prototype would load a byte. */
                velocity.vy = (((s32 (*)(s16*, s16))battle_map_calculate_slope_height)(
                                   &tile_center.vx, unit->battle_data->action_target_elevation)
                                  - unit->screen.vy)
                    << 12;
            }
            velocity.vx /= timer;
            velocity.vy /= timer;
            velocity.vz /= timer;
            unit->distortion_phase++;
        } else {
            unit->distortion_animation_id = 0;
            break;
        }
    case 1:
        if (timer > 0) {
            unit->real.vx += velocity.vx;
            if (allow_height_change) {
                unit->real.vy += velocity.vy;
            }
            unit->real.vz += velocity.vz;
        } else {
            unit->distortion_animation_id = 0;
        }
        timer--;
        break;
    }
    unit->velocity = velocity;
    battle_unit_set_screen_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
    unit->distortion_target = target_z;
}
