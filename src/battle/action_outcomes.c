/* AI simulation brackets, learn-on-hit, steal and war funds rewards, target list sorting and the last used ability. */

#include "fft/ability.h"
#include "fft/action.h"
#include "fft/ai_decision.h"
#include "fft/battle_map.h"
#include "fft/command.h"
#include "fft/formula.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/memory.h"
#include "fft/reaction.h"
#include "fft/reward.h"
#include "fft/script_variable.h"
#include "fft/skillset.h"
#include "fft/status.h"
#include "fft/target.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "fft/weather.h"
#include "psx/libc.h"
#include "psx/types.h"

static u8 g_action_ability_target_list[UNIT_RUNTIME_SLOT_COUNT] = { 0 };

/* Enter AI action simulation, save the current attacker and clear Hamedo state. */
void ai_simulate_begin(void) {
    g_action_state = ACTION_STATE_AI_SIMULATION;
    g_reaction_hamedo_flag = 0;
    mem_copy_byte_data_source_first(
        &g_action_mimic_attacker, &g_ai_mimic_attacker_backup, sizeof(g_action_mimic_attacker));
}

/* End an AI simulation: copy the saved current-attacker block back and return
 * the action state to execute. */
void ai_simulate_end(void) {
    g_action_state = ACTION_STATE_EXECUTE;
    mem_copy_byte_data_source_first(
        &g_ai_mimic_attacker_backup, &g_action_mimic_attacker, sizeof(g_action_mimic_attacker));
}

/* Current weather as seen by damage and movement rules: none when the map
 * ignores weather (indoors), otherwise the weather script variable, with rain
 * and storms moved to the matching snow values on a map flagged for snow. */
s32 weather_get_effective(void) {
    s32 weather = script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
    u32 flags = g_weather_flags;

    if (flags & BATTLE_MAP_WEATHER_FLAG_IGNORE_WEATHER) {
        return WEATHER_NONE;
    }
    if (weather < WEATHER_RAIN) {
        return weather;
    }
    if (flags & BATTLE_MAP_WEATHER_FLAG_SNOW) {
        weather += WEATHER_SNOW_OFFSET;
    }
    return weather;
}

enum {
    ABILITY_LEARN_ON_HIT_ROLL_MAXIMUM = 100,
    ABILITY_LEARN_ON_HIT_ROW_BYTES = 3,
    ABILITY_LEARN_ON_HIT_ABILITY_COUNT = 0x18,
    UNIT_LEARNED_ABILITIES_OFFSET = 0x99
};

/* Find a unit that learns the ability it was just hit with.
 *
 * Only an ability flagged Learn on Hit qualifies, and nothing is learned when
 * Hamedo pre-empted the action. Each unit other than the attacker that the
 * action affected is marked LEARN_CHECKED; a non-monster without a
 * reaction-preventing status learns the ability when it is in the unit's
 * primary skillset and not yet learned in its current job, on a learn-rate
 * roll out of 100. Returns the first learner's slot and writes the ability id
 * through `ability_id`, or -1 when nobody learns it. */
s32 ability_find_learn_on_hit_unit(s32 attacker_id, u16* ability_id) {
    u8 known[ABILITY_LEARN_ON_HIT_ROW_BYTES];
    ability_data_t* ability;
    unit_stats_t* unit;
    s32 i;
    s32 j;
    s32 row;
    s32 byte_index;
    s32 bit;
    u8* scratch;
    u8* src;
    s32 k;
    u8 skillset;

    if (g_reaction_hamedo_flag != 0) {
        return -1;
    }
    if (g_action_actor_used_ability_id == 0) {
        return -1;
    }

    ability = &g_ability_data[g_action_actor_used_ability_id];
    i = 0;
    if (!(ability->type_flags & ABILITY_TYPE_FLAG_LEARN_ON_HIT)) {
        return -1;
    }

    scratch = known;
    do {
        if (attacker_id != i) {
            unit = &g_unit_stats[i];
            if (unit->ability_outcome & UNIT_ABILITY_OUTCOME_AFFECTED) {
                unit->ability_outcome = UNIT_ABILITY_OUTCOME_LEARN_CHECKED;
                if (!(unit->unit_flags & UNIT_FLAG_MONSTER)
                    && status_unit_has_any_in_set(unit, STATUS_CHECK_SET_PREVENT_REACTION) == 0) {
                    skillset = unit->primary_skillset;
                    row = unit->job_id;
                    if (row < JOB_ID_SQUIRE) {
                        row = 0;
                    } else {
                        row = row - JOB_ID_SQUIRE;
                    }
                    /* Typed field access reassociates the 0x99 field offset
                     * with the row and demonstrably breaks the byte-exact
                     * address calculation. Preserve the target's row-offset,
                     * unit-base, then field-offset order. */
                    src = (u8*)((row * ABILITY_LEARN_ON_HIT_ROW_BYTES) + (u32)unit + UNIT_LEARNED_ABILITIES_OFFSET);
                    for (k = 0; k < ABILITY_LEARN_ON_HIT_ROW_BYTES; k++) {
                        scratch[k] = src[k];
                    }

                    /* Ability j of the skillset is learned when bit 7 - j % 8 of
                     * byte j / 8 of the job's row is set. */
                    for (j = 0; j < ABILITY_LEARN_ON_HIT_ABILITY_COUNT; j++) {
                        byte_index = j / 8;
                        bit = 0x80 >> (j - byte_index * 8);
                        if (scratch[byte_index] & bit) {
                            continue;
                        }
                        if ((ability_get_id_from_skillset(skillset, j) & 0xffff) != g_action_actor_used_ability_id) {
                            continue;
                        }
                        if (random_roll_pass_fail(ABILITY_LEARN_ON_HIT_ROLL_MAXIMUM, ability->learn_rate) == 0) {
                            *ability_id = g_action_actor_used_ability_id;
                            return i;
                        }
                    }
                }
            }
        }
        i++;
    } while (i < UNIT_SLOT_COUNT);
    return -1;
}

/* Integer address arithmetic preserves the retail instruction order here. */
#define LEARNED_ABILITIES_OFFSET ((u32) & ((unit_stats_t*)0)->career.learned_abilities)

/* Learn-on-hit: mark the acting unit's used ability as learned in the unit's
 * current job.
 *
 * Walks the 24 abilities of the unit's primary skillset; each one whose id equals
 * g_action_actor_used_ability_id and is not yet learned gets its bit set
 * (MSB first, three bytes per job). A job below Squire uses job slot 0. The
 * learned bits are tested on a copy taken before the walk. */
void ability_learn_used(unit_stats_t* unit) {
    u8 learned[ABILITY_LEARN_ON_HIT_ROW_BYTES];
    u8 skillset;
    s32 job;
    s32 i;
    s32 byte_index;
    s32 mask;
    u8* src;
    u8* dst;
    u8* base;
    u8* learned_byte;

    job = unit->job_id;
    skillset = unit->primary_skillset;
    if (job < JOB_ID_SQUIRE) {
        job = 0;
    } else {
        job -= JOB_ID_SQUIRE;
    }
    /* Three learned-ability bytes per job, from learned_abilities (0x99). */
    dst = learned;
    src = (u8*)(((job * 3) + (s32)unit) + LEARNED_ABILITIES_OFFSET);
    do {
        *dst = *src;
        dst += 1;
        src += 1;
    } while ((s32)dst < (s32)&learned[3]);
    i = 0;
    base = (u8*)(((job * 3) + (s32)unit) + LEARNED_ABILITIES_OFFSET);
    do {
        byte_index = i / 8;
        mask = 0x80 >> (i - (byte_index * 8));
        if (!(learned[byte_index] & mask)) {
            if ((u16)ability_get_id_from_skillset(skillset, i) == g_action_actor_used_ability_id) {
                /* `base[byte_index] |= mask` emits different code. */
                learned_byte = base + byte_index;
                *learned_byte = mask | *learned_byte;
            }
        }
        i += 1;
    } while (i < SKILLSET_ABILITY_LIST_COUNT);
}

/* Pass a status change on to the unit's display flags
 * (status_queue_display_flag_change), but only while an action is
 * executing: previews and AI simulations change no display state.
 *
 * status_id is the status handler index (status id + 1). */
void status_queue_display_flag_change_if_executing(s32 status_id, s32 enabled, s32 battle_id) {
    if (g_action_state == ACTION_STATE_EXECUTE) {
        status_queue_display_flag_change(status_id, enabled, battle_id);
    }
}

/* Give a player-team unit one of item (Steal, Catch, an unbroken Draw Out katana, a treasure
 * chest): with 99 already in stock, the item becomes a quarter of its price in war funds instead.
 * Items 0 and 0xFF, other teams and non-executing states do nothing. */
s32 item_give(unit_stats_t* unit, u8 item) {
    u8 item_id;

    if (g_action_state != ACTION_STATE_EXECUTE) {
        return 0;
    }
    if ((unit->initial_team_flags & UNIT_TEAM_MASK) != UNIT_TEAM_BLUE) {
        return 0;
    }
    item_id = item;
    if (item_id == ITEM_ID_NOTHING) {
        return 0;
    }
    if (item_id == ITEM_ID_NONE) {
        return 0;
    }
    if (item_get_total_quantity(item_id, 1) >= 99) {
        return reward_add_war_funds(unit, 0, item_id);
    }
    g_item_quantities[item_id] += 1;
    return 0;
}

/* Add total to the war funds, plus a quarter of the item's price when an item
 * is given (item_give passes one the stock is full of). Only for
 * a unit currently on the player team and only when executing; the funds stay
 * within 0..99,999,999. Returns the amount added before that clamp. */
s32 reward_add_war_funds(unit_stats_t* unit, s32 total, u8 item_id) {
    s32 value;

    if (g_action_state != ACTION_STATE_EXECUTE) {
        return 0;
    }
    if (unit->team_flags & UNIT_TEAM_MASK) {
        return 0;
    }
    if (item_id != ITEM_ID_NOTHING) {
        total += g_item_primary_data[item_id].price >> 2;
    }
    value = script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) + total;
    if (value > EVENT_WAR_FUNDS_MAX) {
        value = EVENT_WAR_FUNDS_MAX;
    } else if (value < 0) {
        value = 0;
    }
    script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS, value);
    return total;
}

/* Apply an action result's EXP change when executing. Without ACTION_EXP_CHANGE_LOSS
 * the amount is gained: EXP saturates at 255 and the level-up check runs. With it
 * the low seven bits are lost, down to 0. */
void reward_apply_exp_change(unit_stats_t* unit, u8 amount) {
    s32 total;
    u8 capped;
    s16 reduced;

    if (g_action_state != ACTION_STATE_EXECUTE) {
        return;
    }
    if ((amount & ACTION_EXP_CHANGE_LOSS) == 0) {
        total = unit->experience + amount;
        capped = total;
        if (total >= 0x100) {
            capped = 0xFF;
        }
        unit->experience = capped;
        if (unit_check_level_up(unit) != 0) {
            if (unit->battle_id == g_action_actor_id) {
                g_action_current_ability.level_gained_flag = 1;
            }
        }
    } else {
        reduced = unit->experience - (amount & ACTION_EXP_CHANGE_VALUE_MASK);
        if (reduced < 0) {
            reduced = 0;
        }
        unit->experience = reduced;
    }
}

/* Resolve a unit's status changes with the action state set to preview, so no
 * display change is queued, then return to the execute state. */
void status_resolve_unit_changes_in_preview(s32 unit_id, s32 removal_only) {
    g_action_state = ACTION_STATE_PREVIEW;
    status_resolve_unit_changes(unit_id, removal_only);
    g_action_state = ACTION_STATE_EXECUTE;
}

/*
 * Remove the statuses the unit's action marks for removal.
 *
 * Points g_action_target_data at the unit's action data. Only statuses
 * the unit currently has are kept in the removal mask; each one gets its CT
 * cleared and its display removal queued, then the bits are cleared from both
 * the current and innate status sets.
 */
void status_apply_unit_action_removal(unit_stats_t* unit) {
    s32 i;
    s32 battle_id;
    s32 byte_index;
    s32 mask;

    g_action_target_data = &unit->action;
    battle_id = unit->battle_id;
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        g_action_target_data->status_changes.removal[i] &= unit->status_sets.current[i];
    }
    for (i = 0; i < STATUS_COUNT; i++) {
        byte_index = i / 8;
        mask = 0x80 >> (i & 7);
        if (g_action_target_data->status_changes.removal[byte_index] & mask) {
            status_set_ct(unit, i, 0);
            status_queue_display_flag_change_if_executing(STATUS_HANDLER_INDEX(i), 0, battle_id);
        }
    }
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        unit->status_sets.current[i] &= ~g_action_target_data->status_changes.removal[i];
        unit->status_sets.innate[i] &= ~g_action_target_data->status_changes.removal[i];
    }
}

/* Remove a unit from the current ability's target list, shifting later entries down and filling
 * the freed last slot with 0xFF. */
void action_remove_ability_target(s32 target_battle_id) {
    s32 i;

    for (i = 0; i < UNIT_RUNTIME_SLOT_COUNT; i++) {
        if (g_action_ability_target_list[i] == (target_battle_id & 0xFF)) {
            for (; i < UNIT_RUNTIME_SLOT_COUNT - 1; i++) {
                g_action_ability_target_list[i] = g_action_ability_target_list[i + 1];
            }
            g_action_ability_target_list[UNIT_RUNTIME_SLOT_COUNT - 1] = 0xFF;
            return;
        }
    }
}

/* Copy 16 target ids into the current ability's target list. */
void action_store_target_ids(u8* src) {
    s32 i;

    for (i = 0; i < UNIT_RUNTIME_SLOT_COUNT; i++) {
        g_action_ability_target_list[i] = src[i];
    }
}

/* Copy the current ability's 16-entry target list to `out` and return how many
 * entries hold a unit (anything but 0xff). */
s32 action_copy_ability_targets(u8* out) {
    s32 count = 0;
    s32 i;

    for (i = 0; i < UNIT_RUNTIME_SLOT_COUNT; i++) {
        u8 target_id = g_action_ability_target_list[i];
        *out++ = target_id;
        if (target_id != 0xFF) {
            count++;
        }
    }
    return count;
}

/* Store value in g_action_acting_unit_value when unit_id is the acting unit of a primary
 * (non-reaction) action. */
void action_store_acting_unit_value(s32 unit_id, s8 value) {
    if (g_action_context == ACTION_CONTEXT_PRIMARY && unit_id == g_action_actor_id) {
        g_action_acting_unit_value = value;
    }
}

/* Random value 0..0x7FFF for the formulas while the action executes; previews and AI simulations
 * get the midpoint 0x4000, so projected results use an average roll. */
s32 formula_get_random(void) {
    if (g_action_state != ACTION_STATE_EXECUTE) {
        return 0x4000;
    }
    return rand();
}

/* Return 1 when a random value in [0, limit) is at least threshold, otherwise 0. */
s32 formula_roll_random_at_least(s32 limit, s32 threshold) {
    return (formula_get_random() * limit) / 32768 >= threshold;
}

/* Empty. Called when the acting unit's own action has ended, with its stats and the quadrant
 * (0 to 3) its sprite faces. */
void action_post_action_facing_stub(unit_stats_t* unused_unit, s32 unused_facing) {
}

/* Record the acting unit's action for Mimes to copy: the action itself, the
 * target's offset from the unit, the unit's facing, team and weapons.
 *
 * A Mime's own action is not recorded. For a tile target only the low byte of
 * each coordinate is used. */
void action_mimic_record(unit_stats_t* unit) {
    unit_stats_t* target;
    u8 x;
    u8 y;

    if (unit->job_id == JOB_ID_MIME) {
        return;
    }
    action_copy_data(&unit->action_command, &g_action_mimic_attacker.action);
    if (unit->action_command.targeting_type == TARGET_TYPE_UNIT) {
        target = &g_unit_stats[unit->action_command.target_id];
        x = target->x;
        y = target->position.y;
    } else {
        /* Only the low byte of the s16 target coordinates is read. */
        x = unit->action_command.target_position.vx;
        y = unit->action_command.target_position.vz;
    }
    g_action_mimic_attacker.target_delta_x = x - unit->x;
    g_action_mimic_attacker.target_delta_y = y - unit->position.y;
    g_action_mimic_attacker.facing = unit->position.facing;
    g_action_mimic_attacker.team = unit->team_flags & UNIT_TEAM_MASK;
    g_action_mimic_attacker.right_hand_weapon_id = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
    g_action_mimic_attacker.left_hand_weapon_id = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
    g_action_mimic_attacker.mimic_pending = 1;
}

/* Set up a Mime's copy of the recorded action (g_action_mimic_attacker).
 * The Mime must share the actor's team, not be the actor and not be prevented
 * from reacting; a default, Arithmeticks or monster ability flagged Cannot
 * Mimic is refused. The Mime repeats the action from its own tile, with the
 * target offset turned by its facing relative to the actor's, and takes the
 * actor's weapons.
 *
 * Returns 0 for an ineligible unit, -1 if targeting or preview fails, and 1
 * on success. Targeting fails when the panel routine finds no target, or when
 * the copied target tile is outside the ability's range; failed targeting or
 * preview clears both borrowed weapon slots. The preview runs in the preview
 * action state and then drops any charge, Jump or Performing it started.
 *
 * The menu type stays byte-sized, and the facing subtraction and action-field
 * assignments retain the target's evaluation order. The tile index and saved
 * action state share a temporary because their lifetimes do not overlap.
 */
s32 action_mimic_set_ability(unit_stats_t* unit) {
    action_command_t action;
    action_command_t* current;
    s32 x;
    s32 y;
    s32 z;
    s32 direction;
    u8 menu_type;
    s32 result;
    s32 saved_state;
    s32 weapon_id;
    s32 unit_id;
    s32 attacker_facing;
    s32 ability_id;
    s32 facing;

    current = &g_action_mimic_attacker.action;
    if (reaction_is_prevented(unit) != 0) {
        return 0;
    }
    if ((unit->team_flags & UNIT_TEAM_MASK) != g_action_mimic_attacker.team) {
        return 0;
    }
    if (unit->battle_id == current->actor_id) {
        return 0;
    }
    menu_type = g_command_menu_types_by_skillset[current->skillset_id];
    /* The target loads the ability id signed (lh). */
    ability_id = g_action_mimic_attacker.action.ability_id;
    if (menu_type == ACTION_MENU_TYPE_DEFAULT || menu_type == ACTION_MENU_TYPE_ARITHMETICKS
        || menu_type == ACTION_MENU_TYPE_MONSTER) {
        if (g_ability_range_data[ability_id].flags_3 & ABILITY_SECONDARY_FLAG_3_CANNOT_MIMIC) {
            return 0;
        }
    }
    /* Quarter turns from the actor's facing to the Mime's, kept non-negative by
     * the extra 4; a Jump picks its landing layer by height (bit 8). */
    facing = unit->position.facing;
    attacker_facing = g_action_mimic_attacker.facing - 4;
    direction = facing - attacker_facing;
    if (menu_type == ACTION_MENU_TYPE_JUMP) {
        direction |= 0x100;
    }
    if (target_resolve_rotated_offset_layer(
            unit, g_action_mimic_attacker.target_delta_x, g_action_mimic_attacker.target_delta_y, direction, &x, &y, &z)
        != 0) {
        return -1;
    }
    action_copy_data(current, &action);
    action.targeting_type = TARGET_TYPE_TILE;
    action.actor_id = unit->battle_id;
    unit_id = unit->battle_id;
    action.target_position.vx = x;
    action.target_position.vy = z;
    weapon_id = g_action_mimic_attacker.right_hand_weapon_id;
    action.target_position.vz = y;
    action.target_id = unit_id;
    unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = weapon_id;
    unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = g_action_mimic_attacker.left_hand_weapon_id;
    result = target_range_set_for_action(&action);
    /* A byte flag test retains the original compound-condition scheduling. */
    if (result == TARGET_PANELS_INVALID || result == TARGET_PANELS_NONE
        || (result < TARGET_PANELS_NO_TARGETING
            && !((*(u8*)&g_battle_map_tile_data[saved_state = (z << 8) + (g_battle_map_max_x * y) + x]
                         .ceiling_depth_and_marks)
                & BATTLE_MAP_TILE_FLAG_ABILITY_RANGE))) {
        unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = ITEM_ID_NONE;
        unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = ITEM_ID_NONE;
        return -1;
    }
    saved_state = g_action_state;
    g_action_state = ACTION_STATE_PREVIEW;
    result = action_prepare(&action, &unit->action_command, 1);
    unit->charged_ability_ct = 0xff;
    status_change_unit(unit, 0,
        STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_JUMP)
            | STATUS_BYTE_MASK(STATUS_ID_PERFORMING),
        STATUS_REMOVE);
    g_action_state = saved_state;
    if (result < 0) {
        unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = ITEM_ID_NONE;
        unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = ITEM_ID_NONE;
        return -1;
    }
    return 1;
}

/* Rotate a tile offset around the unit and select the destination's terrain
 * layer.
 *
 * The low two bits of `flags` turn (dx, dy) by that many quarter turns. Returns
 * -1 when the rotated offset leaves the map or lands where both layers are
 * blocked; otherwise 0 with `*out_flag` selecting the upper (1) or lower (0)
 * terrain layer. With one layer blocked the other is chosen. Bit 8 of `flags`
 * picks the higher layer outright; otherwise the layer nearer the height of the
 * unit's own tile wins, ties going to the higher one. An upper-layer tile sits
 * 0x100 entries after its lower-layer tile in g_battle_map_tile_data.
 */
s32 target_resolve_rotated_offset_layer(
    unit_stats_t* unit, s32 dx, s32 dy, s32 flags, s32* out_x, s32* out_y, s32* out_flag) {
    battle_map_tile_t* current;
    battle_map_tile_t* lower;
    battle_map_tile_t* upper;
    s32 index;
    s32 upper_index;
    s32 lower_distance;
    s32 upper_distance;
    s32 previous;
    u8 current_height;
    u8 lower_height;
    /* The upper tile's blocked bit, then its height: one byte variable, which is
     * why the first test of the bit keeps its andi 0xff. */
    u8 upper_value;

    switch (flags & 3) {
    case 1:
        previous = dx;
        dx = dy;
        dy = -previous;
        break;
    case 2:
        dx = -dx;
        dy = -dy;
        break;
    case 3:
        previous = dx;
        dx = -dy;
        dy = previous;
        break;
    }
    *out_x = unit->x + dx;
    *out_y = unit->position.y + dy;
    if (*out_x < 0 || *out_x >= g_battle_map_max_x) {
        return -1;
    }
    if (*out_y < 0 || *out_y >= g_battle_map_max_y) {
        return -1;
    }
    current = &g_battle_map_tile_data[battle_map_tile_calculate_unit_index(unit)];
    index = g_battle_map_max_x * *out_y + *out_x;
    upper_index = index + 0x100;
    upper = &g_battle_map_tile_data[upper_index];
    lower = &g_battle_map_tile_data[index];
    upper_value = upper->flags.blocked;
    if ((lower->flags.blocked) != 0) {
        if (upper_value != 0) {
            return -1;
        }
        *out_flag = 1;
        return 0;
    }
    if (upper_value != 0) {
        *out_flag = 0;
        return 0;
    }
    upper_value = upper->height;
    current_height = current->height;
    lower_height = lower->height;
    if ((flags & 0x100) != 0) {
        if (upper_value < lower_height) {
            *out_flag = 0;
            return 0;
        }
        *out_flag = 1;
        return 0;
    }
    if (lower_height > current_height) {
        lower_distance = lower_height - current_height;
    } else {
        lower_distance = current_height - lower_height;
    }
    if (upper_value > current_height) {
        upper_distance = upper_value - current_height;
    } else {
        upper_distance = current_height - upper_value;
    }
    if (upper_distance < lower_distance) {
        *out_flag = 1;
        return 0;
    }
    if (lower_distance < upper_distance) {
        *out_flag = 0;
        return 0;
    }
    if (upper_value < lower_height) {
        *out_flag = 0;
    } else {
        *out_flag = 1;
    }
    return 0;
}

/* Return the saved action's ability. Nothing in the game calls it. */
s16 ability_load_last_used(void) {
    return g_action_saved_command.ability_id;
}
