#include "fft/battle_gfx.h"
#include "fft/battle_move.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/*
 * Select a unit's resting animation from its renderer-side status mirror.
 *
 * In water less than two deep a crystal or a treasure shows its own
 * animation. Any other unit takes the animation of the first of: dead,
 * riding, critical (the kneel of sleep; not for a mount), stop, sleep,
 * petrify, confusion, performing or charging (the hold animation of the
 * ability's charge set), defending, slow, haste and cursed; with none of
 * them it idles. The RUKA, ARUTE and KANZEN sequences have no
 * riding or critical animation, and a sheet with the flying flag idles with
 * the three flying animations. In deeper water the choice is the dead, the
 * riding or one of the submerged idle animations.
 *
 * Animation 0x21 belongs to bit 0 of the 32-bit status mask, which no status maps
 * to (g_battle_unit_status_mask_by_handler_index): that arm never runs.
 */
void battle_unit_set_animation_based_on_status(battle_unit_t* unit) {
    s32 animation;
    u8 water_depth;
    u8 spritesheet_id;

    water_depth = move_get_water_depth(unit);
    animation = 0;
    switch (water_depth) {
    case 0:
    case 1:
        if (unit->status_flags_5_6 & BATTLE_UNIT_STATUS_CRYSTAL) {
            animation = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE;
        } else if (unit->status_flags_5_6 & BATTLE_UNIT_STATUS_TREASURE) {
            animation = BATTLE_UNIT_ANIMATION_TREASURE;
        } else if ((unit->status_flags_1_4 & BATTLE_UNIT_STATUS_ANIMATION_SELECTION_MASK) == 0) {
            animation = BATTLE_UNIT_ANIMATION_IDLE_FLYING;
            if (battle_gfx_get_spritesheet_flying_flag(unit->spritesheet_id) == 0) {
                animation = BATTLE_UNIT_ANIMATION_IDLE;
            }
        } else {
            spritesheet_id = unit->spritesheet_id;
            if (g_battle_gfx_spritesheet_data[spritesheet_id].seq_id >= BATTLE_SEQ_RUKA) {
                if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_DEAD) {
                    animation = BATTLE_UNIT_ANIMATION_DEAD;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_STOP) {
                    animation = BATTLE_UNIT_ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_SLEEP) {
                    animation = BATTLE_UNIT_ANIMATION_SLEEP;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_PETRIFY) {
                    animation = BATTLE_UNIT_ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_CONFUSION) {
                    animation = BATTLE_UNIT_ANIMATION_CONFUSION;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_PERFORMING) {
                    unit->used_ability_id = unit->stats->action_command.ability_id;
                    battle_unit_start_ability_charge_hold_animation(unit);
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_CHARGING) {
                    unit->used_ability_id = unit->stats->action_command.ability_id;
                    battle_unit_start_ability_charge_hold_animation(unit);
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_DEFENDING) {
                    animation = BATTLE_UNIT_ANIMATION_DEFEND;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_SLOW) {
                    animation = BATTLE_UNIT_ANIMATION_IDLE_FLYING_SLOW;
                    if (battle_gfx_get_spritesheet_flying_flag(unit->spritesheet_id) == 0) {
                        animation = BATTLE_UNIT_ANIMATION_IDLE_SLOW;
                    }
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_HASTE) {
                    animation = BATTLE_UNIT_ANIMATION_IDLE_FLYING_HASTE;
                    if (battle_gfx_get_spritesheet_flying_flag(unit->spritesheet_id) == 0) {
                        animation = BATTLE_UNIT_ANIMATION_IDLE_HASTE;
                    }
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_UNNAMED_0E) {
                    animation = BATTLE_UNIT_ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & 1) {
                    animation = 0x21;
                } else {
                    animation = BATTLE_UNIT_ANIMATION_IDLE_FLYING;
                    if (battle_gfx_get_spritesheet_flying_flag(unit->spritesheet_id) == 0) {
                        animation = BATTLE_UNIT_ANIMATION_IDLE;
                    }
                }
            } else {
                if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_DEAD) {
                    animation = BATTLE_UNIT_ANIMATION_DEAD;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_MOUNTED) {
                    animation = BATTLE_UNIT_ANIMATION_RIDING;
                } else if ((unit->status_flags_1_4 & BATTLE_UNIT_STATUS_CRITICAL)
                    && unit->mount_state != BATTLE_UNIT_MOUNT_STATE_MOUNT) {
                    animation = BATTLE_UNIT_ANIMATION_SLEEP;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_STOP) {
                    animation = BATTLE_UNIT_ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_SLEEP) {
                    animation = BATTLE_UNIT_ANIMATION_SLEEP;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_PETRIFY) {
                    animation = BATTLE_UNIT_ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_CONFUSION) {
                    animation = BATTLE_UNIT_ANIMATION_CONFUSION;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_PERFORMING) {
                    unit->used_ability_id = unit->stats->action_command.ability_id;
                    battle_unit_start_ability_charge_hold_animation(unit);
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_CHARGING) {
                    unit->used_ability_id = unit->stats->action_command.ability_id;
                    battle_unit_start_ability_charge_hold_animation(unit);
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_DEFENDING) {
                    animation = BATTLE_UNIT_ANIMATION_DEFEND;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_SLOW) {
                    animation = BATTLE_UNIT_ANIMATION_IDLE_FLYING_SLOW;
                    if (battle_gfx_get_spritesheet_flying_flag(unit->spritesheet_id) == 0) {
                        animation = BATTLE_UNIT_ANIMATION_IDLE_SLOW;
                    }
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_HASTE) {
                    animation = BATTLE_UNIT_ANIMATION_IDLE_FLYING_HASTE;
                    if (battle_gfx_get_spritesheet_flying_flag(unit->spritesheet_id) == 0) {
                        animation = BATTLE_UNIT_ANIMATION_IDLE_HASTE;
                    }
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_UNNAMED_0E) {
                    animation = BATTLE_UNIT_ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & 1) {
                    animation = 0x21;
                } else {
                    animation = BATTLE_UNIT_ANIMATION_IDLE_FLYING;
                    if (battle_gfx_get_spritesheet_flying_flag(unit->spritesheet_id) == 0) {
                        animation = BATTLE_UNIT_ANIMATION_IDLE;
                    }
                }
            }
        }
        break;
    case 2:
    default:
        if ((unit->status_flags_5_6 & BATTLE_UNIT_STATUS_TRANSFORMATION_MASK) != 0) {
            if (unit->status_flags_5_6 & BATTLE_UNIT_STATUS_CRYSTAL) {
                animation = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE;
            } else if (unit->status_flags_5_6 & BATTLE_UNIT_STATUS_TREASURE) {
                animation = BATTLE_UNIT_ANIMATION_TREASURE;
            }
        } else if ((unit->status_flags_1_4 & BATTLE_UNIT_STATUS_ANIMATION_SELECTION_MASK) != 0) {
            if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_DEAD) {
                animation = BATTLE_UNIT_ANIMATION_DEAD;
            } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_MOUNTED) {
                animation = BATTLE_UNIT_ANIMATION_RIDING;
            } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_HASTE) {
                animation = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE_HASTE;
            } else {
                animation = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE;
                if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_SLOW) {
                    animation = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE_SLOW;
                }
            }
        } else {
            animation = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE;
        }
        break;
    }
    if (animation != 0) {
        battle_unit_store_animation_facing(animation, unit->facing, unit);
    }
}
