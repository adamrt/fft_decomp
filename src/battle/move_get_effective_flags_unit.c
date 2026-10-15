#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/*
 * Combines movement abilities with Float, Frog, and Chicken status effects.
 *
 * The returned byte is consumed as the unit's effective movement-mode flags.
 * Frog and Chicken suppress Fly, while the Float status supplies Float even
 * when it is not equipped as a movement ability.
 */
move_effective_flags_e move_get_effective_flags(const unit_stats_t* unit) {
    u8 flags;
    s32 movement_abilities_2;
    s32 movement_abilities_3;
    s32 current_status_3;

    current_status_3 = unit->status_sets.current[2];
    movement_abilities_3 = unit->ability_flags.movement[2];
    movement_abilities_2 = unit->ability_flags.movement[1];

    flags = 0;
    if (current_status_3 & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
        movement_abilities_3 |= BATTLE_MOVEMENT_SET_3_FLOAT;
    }
    if (current_status_3 & (STATUS_BYTE_MASK(STATUS_ID_CHICKEN) | STATUS_BYTE_MASK(STATUS_ID_FROG))) {
        movement_abilities_3 &= (u8)~BATTLE_MOVEMENT_SET_3_FLY;
    }
    if (movement_abilities_3 & BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER) {
        flags = BATTLE_EFFECTIVE_MOVEMENT_ON_WATER;
    } else if (movement_abilities_3 & BATTLE_MOVEMENT_SET_3_WALK_ON_WATER) {
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
    } else if (movement_abilities_3 & BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER) {
        flags |= BATTLE_EFFECTIVE_MOVEMENT_ON_WATER;
    } else if (movement_abilities_3 & BATTLE_MOVEMENT_SET_3_WALK_ON_WATER) {
        flags |= BATTLE_EFFECTIVE_MOVEMENT_IN_WATER;
    }
    return flags;
}

/* Amount Move-Get EXP or Move-Get JP gives: the number of steps walked
 * (g_move_path.count), rnd(1..Move) for a teleport and 1 for a failed one,
 * at most 99. */
u8 battle_unit_calculate_move_exp_jp_up_to_earn(const unit_stats_t* unit) {
    u8 amount = g_move_path.count;

    if (amount == 0xFE) {
        amount = (rand() * unit->move) / 0x8000 + 1;
    }
    if (amount == 0xFF) {
        amount = 1;
    }
    if (amount >= 0x64) {
        amount = 0x63;
    }
    return amount;
}

/* Run the ability formula for a preview of attacker's command against target.
 *
 * The attacker's 20 command bytes are saved, replaced by command, and
 * restored afterwards. Without the Auto flag in the loaded ability data, the
 * target's tile must be targetable; otherwise its action data is cleared. */
s32 action_calculate_projected_effect(unit_stats_t* attacker, unit_stats_t* target, action_command_t* command) {
    u8 flags_3;
    s32 result;
    s32 saved_action_state;
    s32 saved_action_context;

    main_util_copy_action_data(&attacker->action_command, &g_action_saved_command);
    main_util_copy_action_data(command, &attacker->action_command);
    action_init_current_ability_strike_data(attacker);
    saved_action_state = g_action_state;
    g_action_state = ACTION_STATE_PREVIEW;
    if (!(g_battle_loaded_ability_flags_1 & ABILITY_SECONDARY_FLAG_1_AUTO)) {
        result = target_calculate_map_for_action(command, &flags_3);
        if (result != -1 && !g_map_tile_data[map_calculate_location(target)].ceiling_depth_and_marks.targeted) {
            result = -1;
        }
        if (result == -1) {
            action_clear_current_data(&target->action);
            target->action.hit = 0;
            target->action.attack_accuracy = 0;
            g_action_state = saved_action_state;
            main_util_copy_action_data(&g_action_saved_command, &attacker->action_command);
            return 0;
        }
    }
    saved_action_context = g_action_context;
    g_action_current_ability.defaulted_to_attack = 0;
    g_action_context = ACTION_CONTEXT_PRIMARY;
    target->action.reaction_id = 0;
    action_run_pre_formula_setup(&attacker->action_command, target->battle_id);
    g_action_context = saved_action_context;
    g_action_state = saved_action_state;
    main_util_copy_action_data(&g_action_saved_command, &attacker->action_command);
    return 0;
}

/* The Elemental ability that the surface type of the unit's tile selects. */
s32 action_get_elemental_ability_id(const unit_stats_t* unit) {
    return g_main_terrain_geomancy_abilities[g_map_tile_data[map_calculate_location(unit)].surface.type];
}
