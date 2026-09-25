#include "fft/battle.h"
#include "psx/types.h"

/*
 * Selects the unit animation from its renderer-side status mirror.
 *
 * The masked float/fly result retains the target's unreachable negative test.
 * Animation indices are spritesheet-local and therefore remain literals.
 */
void battle_unit_set_animation_based_on_status(battle_unit_misc_data_t* unit) {
    s32 animation;
    s32 float_fly;
    u8 spritesheet_id;

    float_fly = battle_move_validate_float_fly(unit) & 0xff;
    animation = 0;
    /* A combined 0..1 range test folds to one unsigned compare; the target
     * tests the sign first, so that path jumps into the airborne arm. */
    if (float_fly < 0) {
        goto airborne;
    }
    if (float_fly < 2) {
        if (unit->status_flags_5_6 & BATTLE_MISC_STATUS_CRYSTAL) {
            animation = 9;
        } else if (unit->status_flags_5_6 & BATTLE_MISC_STATUS_TREASURE) {
            animation = 0x15;
        } else if ((unit->status_flags_1_4 & BATTLE_MISC_STATUS_ANIMATION_SELECTION_MASK) == 0) {
            animation = 6;
            if (battle_gfx_get_spritesheet_flying_flag(unit->spritesheet_id) == 0) {
                animation = 3;
            }
        } else {
            spritesheet_id = unit->spritesheet_id;
            if (g_battle_gfx_spritesheet_data[spritesheet_id].seq_id >= 5) {
                if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_DEAD) {
                    animation = 0x1a;
                } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_STOP) {
                    animation = 2;
                } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_SLEEP) {
                    animation = 0x24;
                } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_PETRIFY) {
                    animation = 2;
                } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_CONFUSION) {
                    animation = 0x25;
                } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_PERFORMING) {
                    unit->used_ability_id = unit->battle_data->last_ability_id;
                    battle_unit_start_ability_charge_animation_for_movement(unit);
                } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_CHARGING) {
                    unit->used_ability_id = unit->battle_data->last_ability_id;
                    battle_unit_start_ability_charge_animation_for_movement(unit);
                } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_DEFENDING) {
                    animation = 0x17;
                } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_SLOW) {
                    animation = 8;
                    if (battle_gfx_get_spritesheet_flying_flag(unit->spritesheet_id) == 0) {
                        animation = 5;
                    }
                } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_HASTE) {
                    animation = 7;
                    if (battle_gfx_get_spritesheet_flying_flag(unit->spritesheet_id) == 0) {
                        animation = 4;
                    }
                } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_CURSED) {
                    animation = 2;
                } else if (unit->status_flags_1_4 & 1) {
                    animation = 0x21;
                } else {
                    animation = 6;
                    if (battle_gfx_get_spritesheet_flying_flag(unit->spritesheet_id) == 0) {
                        animation = 3;
                    }
                }
            } else {
                if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_DEAD) {
                    animation = 0x1a;
                } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_MOUNTED) {
                    animation = 0x32;
                } else if ((unit->status_flags_1_4 & BATTLE_MISC_STATUS_CRITICAL)
                    && unit->mount_state != BATTLE_MISC_MOUNT_STATE_MOUNT) {
                    animation = 0x24;
                } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_STOP) {
                    animation = 2;
                } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_SLEEP) {
                    animation = 0x24;
                } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_PETRIFY) {
                    animation = 2;
                } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_CONFUSION) {
                    animation = 0x25;
                } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_PERFORMING) {
                    unit->used_ability_id = unit->battle_data->last_ability_id;
                    battle_unit_start_ability_charge_animation_for_movement(unit);
                } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_CHARGING) {
                    unit->used_ability_id = unit->battle_data->last_ability_id;
                    battle_unit_start_ability_charge_animation_for_movement(unit);
                } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_DEFENDING) {
                    animation = 0x17;
                } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_SLOW) {
                    animation = 8;
                    if (battle_gfx_get_spritesheet_flying_flag(unit->spritesheet_id) == 0) {
                        animation = 5;
                    }
                } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_HASTE) {
                    animation = 7;
                    if (battle_gfx_get_spritesheet_flying_flag(unit->spritesheet_id) == 0) {
                        animation = 4;
                    }
                } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_CURSED) {
                    animation = 2;
                } else if (unit->status_flags_1_4 & 1) {
                    animation = 0x21;
                } else {
                    animation = 6;
                    if (battle_gfx_get_spritesheet_flying_flag(unit->spritesheet_id) == 0) {
                        animation = 3;
                    }
                }
            }
        }
    } else {
    airborne:
        if ((unit->status_flags_5_6 & BATTLE_MISC_STATUS_TRANSFORMATION_MASK) != 0) {
            if (unit->status_flags_5_6 & BATTLE_MISC_STATUS_CRYSTAL) {
                animation = 9;
            } else if (unit->status_flags_5_6 & BATTLE_MISC_STATUS_TREASURE) {
                animation = 0x15;
            }
        } else if ((unit->status_flags_1_4 & BATTLE_MISC_STATUS_ANIMATION_SELECTION_MASK) != 0) {
            if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_DEAD) {
                animation = 0x1a;
            } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_MOUNTED) {
                animation = 0x32;
            } else if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_HASTE) {
                animation = 0xa;
            } else {
                animation = 9;
                if (unit->status_flags_1_4 & BATTLE_MISC_STATUS_SLOW) {
                    animation = 0xb;
                }
            }
        } else {
            animation = 9;
        }
    }
    if (animation != 0) {
        battle_unit_store_animation_facing(animation, (s16)unit->facing, unit);
    }
}
