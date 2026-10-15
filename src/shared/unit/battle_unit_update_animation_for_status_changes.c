#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_move.h"
#include "fft/battle_unit.h"

/*
 * Apply this frame's pending status changes to a unit's animation.
 *
 * A unit that becomes a chicken or a frog starts its idle animation, and one
 * that stops being either the animation its statuses select. A death raises
 * the death smoke for the chocobo and monster sequences and, outside events,
 * the cry of the unit's sex or kind; a new crystal or treasure gets its
 * puff. In an event, gaining and losing the Jump status play the jump's
 * departure and landing. Outside events a death or a revival on land plays
 * BATTLE_UNIT_ANIMATION_DEATH or _REVIVE; in water two or more deep, in
 * events too, the unit is put straight into its dead or its submerged idle
 * sequence, with the wait cleared so that the sequence starts at once.
 */
void battle_unit_update_animation_for_status_changes(battle_unit_t* unit) {
    unit_stats_t* stats;

    if (unit->statuses_to_add_5_6 & BATTLE_UNIT_STATUS_CHICKEN) {
        battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_IDLE, unit->facing, unit);
    } else if (unit->statuses_to_remove_5_6 & BATTLE_UNIT_STATUS_CHICKEN) {
        battle_unit_set_animation_based_on_status(unit);
    }
    if (unit->statuses_to_add_5_6 & BATTLE_UNIT_STATUS_FROG) {
        battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_IDLE, unit->facing, unit);
    } else if (unit->statuses_to_remove_5_6 & BATTLE_UNIT_STATUS_FROG) {
        battle_unit_set_animation_based_on_status(unit);
    }
    if (unit->statuses_to_add_1_4 & BATTLE_UNIT_STATUS_DEAD) {
        if (g_battle_gfx_spritesheet_data[unit->spritesheet_id].seq_id == BATTLE_SEQ_CYOKO
            || g_battle_gfx_spritesheet_data[unit->spritesheet_id].seq_id == BATTLE_SEQ_MON) {
            effect_set_secondary_death_smoke(unit);
        }
        if (g_battle_game_state != BATTLE_GAME_STATE_EVENT) {
            stats = unit->stats;
            if (stats != 0) {
                if (stats->unit_flags & UNIT_FLAG_MALE) {
                    sound_play_movement_sfx(unit, MAIN_SFX_DEATH_MALE);
                } else if (stats->unit_flags & UNIT_FLAG_FEMALE) {
                    sound_play_movement_sfx(unit, MAIN_SFX_DEATH_FEMALE);
                } else if (stats->unit_flags & UNIT_FLAG_MONSTER) {
                    sound_play_movement_sfx(unit, MAIN_SFX_DEATH_MONSTER);
                }
            }
        }
    }
    if (unit->statuses_to_add_5_6 & (BATTLE_UNIT_STATUS_CRYSTAL | BATTLE_UNIT_STATUS_TREASURE)) {
        effect_set_secondary_zodiac_poof(unit);
    }
    if (g_battle_game_state == BATTLE_GAME_STATE_EVENT) {
        if (unit->statuses_to_add_5_6 & BATTLE_UNIT_STATUS_JUMP) {
            battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_JUMP_DEPART, unit->facing, unit);
        } else if (unit->statuses_to_remove_5_6 & BATTLE_UNIT_STATUS_JUMP) {
            battle_unit_draw_and_activate_by_runtime_id(unit->runtime_id);
            battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_JUMP_LAND, unit->facing, unit);
        }
        if ((move_get_water_depth(unit) & 0xff) >= 2) {
            if (unit->statuses_to_add_1_4 & BATTLE_UNIT_STATUS_DEAD) {
                battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_DEAD, unit->facing, unit);
                unit->animation.sequence_id = BATTLE_UNIT_ANIMATION_DEAD << 1;
                unit->animation.wait = 0;
            } else if (unit->statuses_to_remove_1_4 & BATTLE_UNIT_STATUS_DEAD) {
                battle_unit_store_animation_facing_movement_data(
                    BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE, unit->facing, unit);
                unit->animation.sequence_id = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE << 1;
                unit->animation.wait = 0;
            }
        }
    } else if ((move_get_water_depth(unit) & 0xff) < 2) {
        if (unit->statuses_to_add_1_4 & BATTLE_UNIT_STATUS_DEAD) {
            battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_DEATH, unit->facing, unit);
        } else if (unit->statuses_to_remove_1_4 & BATTLE_UNIT_STATUS_DEAD) {
            battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_REVIVE, unit->facing, unit);
        }
    } else if (unit->statuses_to_add_1_4 & BATTLE_UNIT_STATUS_DEAD) {
        battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_DEAD, unit->facing, unit);
        unit->animation.sequence_id = BATTLE_UNIT_ANIMATION_DEAD << 1;
        unit->animation.wait = 0;
    } else if (unit->statuses_to_remove_1_4 & BATTLE_UNIT_STATUS_DEAD) {
        battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE, unit->facing, unit);
        unit->animation.sequence_id = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE << 1;
        unit->animation.wait = 0;
    }
}
