#include "fft/battle.h"

/*
 * Combines movement abilities with Float, Frog, and Chicken status effects.
 *
 * The returned byte is consumed as the unit's effective movement-mode flags.
 * Frog and Chicken suppress Fly, while the Float status supplies Float even
 * when it is not equipped as a movement ability.
 */
battle_move_effective_flags_e battle_move_get_effective_flags(const battle_stats_t* unit) {
    u8 flags;
    s32 movement_abilities_2;
    s32 movement_abilities_3;
    s32 current_status_3;

    current_status_3 = unit->status_sets.current[2];
    movement_abilities_3 = unit->movement_abilities[2];
    movement_abilities_2 = unit->movement_abilities[1];

    flags = 0;
    if (current_status_3 & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FLOAT)) {
        movement_abilities_3 |= BATTLE_MOVEMENT_SET_3_FLOAT;
    }
    if (current_status_3
        & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHICKEN) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG))) {
        movement_abilities_3 &= (u8)~BATTLE_MOVEMENT_SET_3_FLY;
    }
    if (movement_abilities_3 & BATTLE_MOVEMENT_SET_3_WALK_ON_WATER) {
        flags = BATTLE_EFFECTIVE_MOVEMENT_ON_WATER;
    } else if (movement_abilities_3 & BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER) {
        flags = BATTLE_EFFECTIVE_MOVEMENT_IN_WATER;
    }
    if (movement_abilities_3 & BATTLE_MOVEMENT_SET_3_SILENT_WALK) {
        flags |= BATTLE_EFFECTIVE_MOVEMENT_SILENT_WALK;
    }
    if (movement_abilities_3 & BATTLE_MOVEMENT_SET_3_FLY) {
        flags |= BATTLE_EFFECTIVE_MOVEMENT_FLY;
        if (movement_abilities_3 & BATTLE_MOVEMENT_SET_3_FLOAT) {
            /* The target also preserves undefined effective-movement bit 0x04. */
            flags = (flags
                        & (BATTLE_EFFECTIVE_MOVEMENT_FLY | BATTLE_EFFECTIVE_MOVEMENT_TELEPORT | 0x04
                            | BATTLE_EFFECTIVE_MOVEMENT_SILENT_WALK))
                | BATTLE_EFFECTIVE_MOVEMENT_FLOAT;
        }
    } else if (movement_abilities_2 & (BATTLE_MOVEMENT_SET_2_TELEPORT | BATTLE_MOVEMENT_SET_2_TELEPORT_2)) {
        flags |= BATTLE_EFFECTIVE_MOVEMENT_TELEPORT;
        if (movement_abilities_3 & BATTLE_MOVEMENT_SET_3_FLOAT) {
            flags = (flags
                        & (BATTLE_EFFECTIVE_MOVEMENT_FLY | BATTLE_EFFECTIVE_MOVEMENT_TELEPORT | 0x04
                            | BATTLE_EFFECTIVE_MOVEMENT_SILENT_WALK))
                | BATTLE_EFFECTIVE_MOVEMENT_FLOAT;
        }
    } else if (movement_abilities_3 & BATTLE_MOVEMENT_SET_3_FLOAT) {
        /* The target preserves Silent Walk and undefined bit 0x04 here. */
        flags = (flags & 0x06) | BATTLE_EFFECTIVE_MOVEMENT_FLOAT;
    } else if (movement_abilities_3 & BATTLE_MOVEMENT_SET_3_WALK_ON_WATER) {
        flags |= BATTLE_EFFECTIVE_MOVEMENT_ON_WATER;
    } else if (movement_abilities_3 & BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER) {
        flags |= BATTLE_EFFECTIVE_MOVEMENT_IN_WATER;
    }
    return flags;
}
