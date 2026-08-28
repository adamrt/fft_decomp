#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/battle_move.h"
#include "fft/battle_state.h"
#include "psx/types.h"

enum {
    BATTLE_GAME_STATE_34 = 0x34,
};

/*
 * Applies this frame's pending status changes to a unit's renderer state:
 * plays the add/remove animations for the two low status bits, raises the
 * death smoke and movement sound for a newly added status 1-4 bit 2, and then
 * picks the follow-up animation and its countdown from whether the unit is
 * floating or flying.
 */
void battle_unit_update_animation_for_status_changes(battle_unit_misc_data_t* unit) {
    battle_stats_t* stats;

    if (unit->statuses_to_add_5_6 & 2) {
        battle_unit_store_animation_facing_movement_data(3, (s16)unit->facing, unit);
    } else if (unit->statuses_to_remove_5_6 & 2) {
        battle_unit_set_animation_based_on_status(unit);
    }
    if (unit->statuses_to_add_5_6 & 4) {
        battle_unit_store_animation_facing_movement_data(3, (s16)unit->facing, unit);
    } else if (unit->statuses_to_remove_5_6 & 4) {
        battle_unit_set_animation_based_on_status(unit);
    }
    if (unit->statuses_to_add_1_4 & 4) {
        if ((u32)(g_battle_gfx_spritesheet_data[unit->spritesheet_id].seq_id - 2) < 2) {
            battle_effect_set_secondary_death_smoke(unit);
        }
        if (g_battle_game_state != BATTLE_GAME_STATE_34) {
            stats = unit->battle_data;
            if (stats != 0) {
                if (stats->unit_flags & UNIT_FLAG_MALE) {
                    battle_sound_play_movement_sfx(unit, 0x9a);
                } else if (stats->unit_flags & UNIT_FLAG_FEMALE) {
                    battle_sound_play_movement_sfx(unit, 0x45);
                } else if (stats->unit_flags & UNIT_FLAG_MONSTER) {
                    battle_sound_play_movement_sfx(unit, 0x46);
                }
            }
        }
    }
    if (unit->statuses_to_add_5_6 & 9) {
        battle_effect_set_secondary_zodiac_poof(unit);
    }
    if (g_battle_game_state == BATTLE_GAME_STATE_34) {
        if (unit->statuses_to_add_5_6 & 0x40) {
            battle_unit_store_animation_facing_movement_data(0x75, (s16)unit->facing, unit);
        } else if (unit->statuses_to_remove_5_6 & 0x40) {
            battle_unit_animate_and_set_enemy_level_data_by_misc_id(unit->unit_id);
            battle_unit_store_animation_facing_movement_data(0x76, (s16)unit->facing, unit);
        }
        if ((u32)(battle_move_validate_float_fly(unit) & 0xff) >= 2) {
            if (unit->statuses_to_add_1_4 & 4) {
                battle_unit_store_animation_facing_movement_data(0x1a, (s16)unit->facing, unit);
                unit->encoded_animation = 0x34;
                unit->animation_countdown = 0;
            } else if (unit->statuses_to_remove_1_4 & 4) {
                battle_unit_store_animation_facing_movement_data(9, (s16)unit->facing, unit);
                unit->encoded_animation = 0x12;
                unit->animation_countdown = 0;
            }
        }
    } else if ((u32)(battle_move_validate_float_fly(unit) & 0xff) < 2) {
        if (unit->statuses_to_add_1_4 & 4) {
            battle_unit_store_animation_facing(0x34, (s16)unit->facing, unit);
        } else if (unit->statuses_to_remove_1_4 & 4) {
            battle_unit_store_animation_facing(0x35, (s16)unit->facing, unit);
        }
    } else if (unit->statuses_to_add_1_4 & 4) {
        battle_unit_store_animation_facing_movement_data(0x1a, (s16)unit->facing, unit);
        unit->encoded_animation = 0x34;
        unit->animation_countdown = 0;
    } else if (unit->statuses_to_remove_1_4 & 4) {
        battle_unit_store_animation_facing_movement_data(9, (s16)unit->facing, unit);
        unit->encoded_animation = 0x12;
        unit->animation_countdown = 0;
    }
}
