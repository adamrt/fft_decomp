/* Prepare strikes, target lists and reactions and calculate action rewards. */

#include "fft/ability.h"
#include "fft/action.h"
#include "fft/battle_map.h"
#include "fft/command.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/move.h"
#include "fft/reaction.h"
#include "fft/reward.h"
#include "fft/skillset.h"
#include "fft/status.h"
#include "fft/target.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/libc.h"
#include "psx/types.h"

extern unit_stats_t g_unit_stats[UNIT_SLOT_COUNT];
extern u8 g_action_loaded_ability_flags_1;
extern u8 g_action_actor_x;
extern u8 g_action_actor_y;
static u8 s_action_actor_elevation;

/* Dismount destinations reserved since the last strike setup; turn-status
 * and trap results also append. */
extern s32 g_unit_relocated_count;
extern move_tile_coordinates_t g_unit_relocated_coords[];

static s32 mp_check_and_consume(unit_stats_t* unit);

static void draw_out_katana_finalize(unit_stats_t* attacker, action_strike_work_t* work, u8 hit_count);

static void strike_data_init(unit_stats_t* unit);

static s32 auto_potion_item_select(unit_stats_t* unit);

static s32 counter_ability_store(unit_stats_t* unit, s8 skillset_id, s16 ability_id, s32 validate_target);

static s32 reflect_apply(unit_stats_t* unit);

static s32 units_on_panels_list(u8* list, const unit_stats_t* origin);

static void random_fire_tile_select(void);

static void strike_coordinates_set(const unit_stats_t* acting, action_strike_work_t* out);

/* Build a unit's targeting-height record.
 *
 * A unit is 6 half heights tall, 4 as a Chicken or Frog, and 2 more when
 * another unit shares its x, y and elevation (a rider and its mount); for a
 * non-Chocobo that other unit then supplies the terrain, status and movement
 * data. Float and the water movement abilities set the walking height on a
 * water tile. A dead or absent unit, a crystal or a treasure is marked
 * UNIT_HEIGHT_UNAVAILABLE.
 *
 * `id` holds the ENTD slot and then the job, and `level` the elevation and
 * then the walking height. Each shared variable is required: `id` conflicts
 * with the 0xff constant in $v0, which puts the job load in $v1, and `level`
 * outranks `depth` in allocation, which gives it $v1 and `depth` $a1.
 */
unit_height_data_t* unit_action_height_calculate(unit_height_data_t* out, s32 unit_id) {
    unit_height_data_t height;
    unit_stats_t* unit;
    unit_stats_t* candidate;
    battle_map_tile_t* tile;
    s32 offset;
    s32 i;
    s32 id;
    u8 flags;
    u8 x;
    u8 y;
    u8 level;
    s32 tile_height;
    s32 depth;
    s32 water_offset;

    offset = 0;
    unit = &g_unit_stats[unit_id];
    if (unit_id >= 0 && unit_id < UNIT_SLOT_COUNT && (id = unit->active_battle_id) != UNIT_ACTIVE_BATTLE_ID_NONE
        && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))
        && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
        flags = unit_id;
        id = unit->job_id;
        if (id < JOB_ID_CHOCOBO || id > JOB_ID_RED_CHOCOBO)
            flags = unit_id | UNIT_HEIGHT_NON_CHOCOBO;
        x = unit->x;
        height.x = x;
        y = unit->position.y;
        height.y = y;
        level = unit->position.higher_elevation;
        height.elevation = level;
        for (i = 0; i < UNIT_SLOT_COUNT; i++) {
            candidate = &g_unit_stats[i];
            if (i != unit_id && candidate->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE && candidate->x == x
                && candidate->position.y == y && candidate->position.higher_elevation == level) {
                if (flags & UNIT_HEIGHT_NON_CHOCOBO)
                    unit = candidate;
                offset = 2;
                flags |= UNIT_HEIGHT_SHARED_TILE;
                break;
            }
        }
        height.unit_flags = flags;
        flags = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHICKEN)];
        if (flags & (STATUS_BYTE_MASK(STATUS_ID_CHICKEN) | STATUS_BYTE_MASK(STATUS_ID_FROG)))
            height.unit_height = offset + 4;
        else
            height.unit_height = offset + 6;
        flags = unit->ability_flags.movement[2];
        tile = &g_battle_map_tile_data[battle_map_tile_calculate_unit_index(unit)];
        /* Byte access retains the original packed load and field-extraction order. */
        tile_height = tile->height * 2 + ((*(u8*)&tile->depth_half_height) & BATTLE_MAP_TILE_HALF_HEIGHT_MASK);
        depth = (*(u8*)&tile->depth_half_height) >> BATTLE_MAP_TILE_DEPTH_SHIFT;
        level = tile_height;
        if (depth != 0) {
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
                water_offset = depth * 2 + 2;
                level = tile_height + water_offset;
            } else if (flags & UNIT_MOVEMENT_SET_3_MOVE_IN_WATER) {
                level = tile_height + depth * 2;
            } else if (flags & UNIT_MOVEMENT_SET_3_WALK_ON_WATER) {
                water_offset = depth * 2 - 2;
                level = tile_height + water_offset;
            }
        } else if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
            level = tile_height + 2;
        }
        height.walking_height = level;
        height.total_height = level + height.unit_height;
        if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD))
            height.unit_flags = UNIT_HEIGHT_UNAVAILABLE;
    } else {
        height.unit_flags = UNIT_HEIGHT_UNAVAILABLE;
    }
    *out = height;
    return out;
}

/* Validate the action in place with action_prepare phase 0, which
 * consumes nothing and leaves the unit's turn flags alone. */
void action_prepare_in_place(action_command_t* action) {
    action_prepare(action, action, 0);
}

/* Run action_prepare phase 1 on a command, writing the prepared
 * action into the acting unit's own action fields.
 *
 * An action that resolves immediately also ends the unit's Charging, Jump,
 * Defending and Performing statuses, but only in the primary action context.
 * Returns the action_preparation_e result. */
action_preparation_e action_commit_preparation(const action_command_t* command) {
    unit_stats_t* unit = &g_unit_stats[command->actor_id];
    action_preparation_e result = action_prepare(command, &unit->action_command, 1);

    if (result == ACTION_PREPARATION_IMMEDIATE && g_action_context == ACTION_CONTEXT_PRIMARY) {
        status_clear_action_state(unit);
    }
    return result;
}

/*
 * Validate an action and copy it to dest with its resolved ability id. Item,
 * Throw, Math Skill and Attack resolve at once; Jump stores a CT of 50 / Speed
 * (at least 1); Charge takes its table CT; other abilities take their CT, halved
 * (rounded up) by Short Charge and zeroed by Non-Charge unless they are
 * performances (Persevere). A nonzero phase stores the CT; an odd phase also
 * commits the action (spends the item, marks the unit as acted, starts Charging,
 * Performing or Jumping). Returns a action_preparation_e code: an
 * ordinary ability returns IMMEDIATE with a CT of 0 and CHARGE otherwise.
 *
 * The one-trip loop around `skip = 1` counts that set one loop level deeper,
 * so global-alloc ranks `skip` above `persevere` and gives them s6/s7 as in
 * the target.
 */
action_preparation_e action_prepare(const action_command_t* source, action_command_t* dest, s32 phase) {
    action_command_t action;
    unit_stats_t* unit;
    ability_secondary_data_t* entry;
    s16 ability;
    u8* table;
    u8 jump_ct;
    u8 item;
    u8 ct;
    u8 persevere;
    s32 skip;
    u16 type;
    u16 multiplier;
    u8 support;

    action_command_copy(source, &action);
    if (action.actor_id >= UNIT_SLOT_COUNT) {
        return ACTION_PREPARATION_INVALID;
    }

    unit = &g_unit_stats[action.actor_id];
    if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
        return ACTION_PREPARATION_INVALID;
    }

    skip = 0;
    if (action.targeting_type == TARGET_TYPE_UNIT) {
        if (action.target_id >= UNIT_SLOT_COUNT) {
            return ACTION_PREPARATION_INVALID;
        }
        if (g_unit_stats[action.target_id].active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
            return ACTION_PREPARATION_INVALID;
        }
    }

    /* Menus that resolve at once return here; the others break with the ability for the
     * CT step below. */
    switch (g_command_menu_types_by_skillset[action.skillset_id]) {
    case ACTION_MENU_TYPE_ITEM_INVENTORY:
        item = action.item_id;
        if (g_item_primary_data[item].type != ITEM_TYPE_CONSUMABLE) {
            return ACTION_PREPARATION_INVALID;
        }
        action_command_copy(&action, dest);
        ability = item + (ABILITY_ID_ITEM_FIRST - ITEM_ID_CONSUMABLE_FIRST);
        dest->ability_id = ability;
        if (phase != 0 && (phase & 1)) {
            item_decrement_party_quantity(unit, item, 1);
            unit->action_taken = 1;
        }
        return ACTION_PREPARATION_IMMEDIATE;
    case ACTION_MENU_TYPE_WEAPON_INVENTORY:
        /* Throw resolves to the Throw ability whose item type matches the thrown item. */
        ability = ABILITY_ID_THROW_FIRST;
        item = action.item_id;
        table = g_ability_throw_item_types - ABILITY_ID_THROW_FIRST;
        for (; ability < ABILITY_ID_JUMP_FIRST; ability++) {
            /* The index+base integer spelling of the biased table is byte-required. */
            if (*(u8*)(ability + (s32)table) == g_item_primary_data[item].type) {
                break;
            }
        }
        if (ability == ABILITY_ID_JUMP_FIRST) {
            return ACTION_PREPARATION_INVALID;
        }
        action_command_copy(&action, dest);
        dest->ability_id = ability;
        if (phase != 0 && (phase & 1)) {
            item_decrement_party_quantity(unit, item, 1);
            unit->action_taken = 1;
        }
        return ACTION_PREPARATION_IMMEDIATE;
    case ACTION_MENU_TYPE_MATH_SKILL:
        ability = action.ability_id;
        type = action.calculator_type;
        multiplier = action.calculator_multiplier;
        if (type < ABILITY_ID_MATH_FIRST || type >= ABILITY_ID_REACTION_FIRST) {
            return ACTION_PREPARATION_INVALID;
        }
        if (multiplier < ABILITY_ID_MATH_FIRST || multiplier >= ABILITY_ID_REACTION_FIRST) {
            return ACTION_PREPARATION_INVALID;
        }
        if (ability < 0 || ability >= ABILITY_ID_ITEM_FIRST) {
            return ACTION_PREPARATION_INVALID;
        }
        action_command_copy(&action, dest);
        if (phase != 0 && (phase & 1)) {
            unit->action_taken = 1;
        }
        return ACTION_PREPARATION_IMMEDIATE;
    case ACTION_MENU_TYPE_ELEMENTS:
        action.ability_id = ability_get_elemental_id(unit);
        break;
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        item = action.item_id;
        if (item < ITEM_ID_ASURA_KNIFE || item > ITEM_ID_CHIRIJIRADEN) {
            return ACTION_PREPARATION_INVALID;
        }
        ability = item + (ABILITY_ID_DRAW_OUT_ASURA - ITEM_ID_ASURA_KNIFE);
        action.ability_id = ability;
        /* The katana is spent here; action_apply_target_result gives it
         * back when it does not break. */
        if (phase & 1) {
            item_decrement_party_quantity(unit, item, 1);
        }
        break;
    case ACTION_MENU_TYPE_ATTACK:
        action_command_copy(&action, dest);
        if (phase != 0 && (phase & 1)) {
            unit->action_taken = 1;
        }
        dest->ability_id = 0;
        return ACTION_PREPARATION_IMMEDIATE;
    case ACTION_MENU_TYPE_JUMP:
        action_command_copy(&action, dest);
        if (phase != 0) {
            ct = 50 / unit->attributes[UNIT_ATTRIBUTE_SPEED];
            if (ct == 0) {
                jump_ct = 1;
            } else {
                jump_ct = ct;
            }
            unit->charged_ability_ct = jump_ct;
            if (phase & 1) {
                unit->action_taken = 1;
                unit->movement_taken = 1;
                status_action_state_set(unit, STATUS_ACTION_STATE_JUMPING);
            }
        }
        dest->ability_id = ABILITY_ID_JUMP_FIRST;
        return ACTION_PREPARATION_JUMP;
    case ACTION_MENU_TYPE_CHARGE:
        ability = action.ability_id;
        if (ability < ABILITY_ID_CHARGE_FIRST || ability >= ABILITY_ID_MATH_FIRST) {
            return ACTION_PREPARATION_INVALID;
        }
        action_command_copy(&action, dest);
        ct = g_ability_charge_data[ability - ABILITY_ID_CHARGE_FIRST].ct;
        /* Charge has its own CT, so the generic CT step and Short Charge are skipped. */
        do {
            skip = 1;
        } while (0);
        if (phase & 1) {
            unit->movement_taken = 1;
        }
        persevere = 0;
        break;
    case ACTION_MENU_TYPE_DEFEND:
        return ACTION_PREPARATION_CHARGE;
    case ACTION_MENU_TYPE_CHANGE_EQUIPMENT:
        return ACTION_PREPARATION_CHANGE_EQUIPMENT;
    case ACTION_MENU_TYPE_DEFAULT:
    case ACTION_MENU_TYPE_MONSTER:
        break;
    default:
        return ACTION_PREPARATION_INVALID;
    }

    if (!skip) {
        ability = action.ability_id;
        if (ability < 0 || ability >= ABILITY_ID_ITEM_FIRST) {
            return ACTION_PREPARATION_INVALID;
        }
        action_command_copy(&action, dest);
        entry = &g_ability_range_data[ability];
        /* Bit 7 is masked off here, though the AI reads the whole byte; no retail record sets it. */
        ct = entry->ct & 0x7f;
        persevere = entry->flags_3 & ABILITY_SECONDARY_FLAG_3_PERSEVERE;
        support = unit->ability_flags.support[3];
        if (!persevere) {
            if (support & UNIT_SUPPORT_SET_4_SHORT_CHARGE) {
                ct = (ct + 1) >> 1;
            }
            if (support & UNIT_SUPPORT_SET_4_NON_CHARGE) {
                ct = 0;
            }
        }
    }

    if (phase != 0) {
        unit->charged_ability_ct = ct;
        if (phase & 1) {
            /* Outside the primary action context the unit is not marked as acted or charging. */
            if (g_action_context != ACTION_CONTEXT_PRIMARY) {
                return ct == 0;
            }
            unit->action_taken = 1;
            if (ct != 0) {
                if (persevere) {
                    status_action_state_set(unit, STATUS_ACTION_STATE_PERFORMING);
                } else {
                    status_action_state_set(unit, STATUS_ACTION_STATE_CHARGING);
                }
            }
        }
    }

    return ct == 0;
}

/* Return an occupied unit slot's action command, or 0 for an
 * empty slot or an id past the last slot. No retail code calls it. */
action_command_t* unit_action_command_get(s32 battle_id) {
    if (battle_id < UNIT_SLOT_COUNT) {
        if (g_unit_stats[battle_id].active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE) {
            return &g_unit_stats[battle_id].action_command;
        }
    }
    return 0;
}

/* Check, and optionally spend, one of an inventory item for a unit that started
 * the battle on the blue team; other teams have unlimited stock.
 *
 * Returns -1 when the party owns none, otherwise 0. One item is removed only when
 * spend is nonzero and the action is executing (not a preview or an AI
 * simulation); with spend 0 the call only tests the stock. */
s32 item_decrement_party_quantity(unit_stats_t* unit, s32 item_id, s32 spend) {
    u32 item;
    s32 quantity;

    if ((unit->initial_team_flags & UNIT_TEAM_MASK) == UNIT_TEAM_BLUE) {
        item = item_id & 0xFF;
        quantity = g_item_quantities[item];
        if (quantity == 0) {
            return -1;
        }
        if (spend == 0) {
            return 0;
        }
        if (g_action_state != ACTION_STATE_EXECUTE) {
            return 0;
        }
        g_item_quantities[item] = quantity - 1;
    }
    return 0;
}

/* Start resolving a unit's queued action: check and spend its MP, record it as
 * the acting unit with its last ability, clear every unit's reaction and outcome
 * and set up the first strike.
 *
 * Returns 0 when the action can run, -1 for an empty unit slot, ACTION_REJECTION_HAMEDO
 * when Hamedo pre-empted the action (g_reaction_hamedo_flag set), or the nonzero
 * action_rejection_e of mp_check_and_consume; on those failures only
 * the acting-unit data is stored. */
s32 action_init_current_data(s32 id) {
    unit_stats_t* unit;
    s32 i;
    unit_stats_t* base;
    s32 mp_result;
    u16 ability;

    g_action_actor_id = -1;
    g_action_context = ACTION_CONTEXT_PRIMARY;
    if (id >= UNIT_SLOT_COUNT) {
        return -1;
    }
    unit = &g_unit_stats[id];
    base = g_unit_stats; /* indexing the global in the loop below is 8 bytes longer */
    if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
        return -1;
    }
    if (g_reaction_hamedo_flag != 0) {
        action_store_actor_data(unit);
        return ACTION_REJECTION_HAMEDO;
    }
    mp_result = mp_check_and_consume(unit);
    if (mp_result != 0) {
        action_store_actor_data(unit);
        return mp_result;
    }
    /* The target loads the s16 action_command.ability_id unsigned (lhu). */
    ability = unit->action_command.ability_id;
    g_action_actor_id = id;
    g_action_actor_used_ability_id = ability;
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        base[i].action.reaction_id = 0;
        base[i].ability_outcome = 0;
    }
    strike_data_init(unit);
    action_mimic_record(unit);
    return 0;
}

/*
 * Build the target list for a reaction ability.
 *
 * Reflect redirects to the unit on the action tile (the rider of a mounted
 * pair), Distribute selects the injured units whose original team is the
 * reactor's current team, and Damage Split selects the acting unit. The remaining
 * reactions follow the target policy stored in the reaction flags table.
 * Returns 0 only for a reaction that targets the attacker, which the caller
 * then targets through the ordinary panels.
 */
static s32 reaction_targets_build(s32 actor_id, action_strike_work_t* work, s32* target_count, u8* target_ids) {
    unit_stats_t* actor = &g_unit_stats[actor_id];
    unit_stats_t* unit_base = g_unit_stats;
    unit_stats_t* unit;
    s32 tile_index;
    /* One variable: the Reflect target id, then the Distribute target count. */
    s32 target_or_count;
    u8 flags;

    switch (g_reaction_ability_id) {
    case ABILITY_ID_REACTION_REFLECT: {
        u8 target_x = actor->action_command.target_position.vx;
        u8 target_y;
        u8 target_elevation;
        u8 mount_info;

        work->target_tile.x = target_x;
        target_y = actor->action_command.target_position.vz;
        work->target_tile.y = target_y;
        target_elevation = actor->action_command.target_position.vy;
        work->target_tile.layer = target_elevation;
        target_or_count = target_find_unit_at_tile(target_x, target_y, target_elevation);
        if (target_or_count < 0) {
            *target_count = 0;
            break;
        }
        unit = &unit_base[target_or_count];
        mount_info = unit->mount_info;
        if (mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT) {
            target_or_count = mount_info & UNIT_MOUNT_INFO_PARTNER_ID_MASK;
        }
        if (target_is_unit_untargetable_and_store_tile(target_or_count, &tile_index) != 0) {
            *target_count = 0;
            break;
        }
        *target_count = 1;
        *target_ids = target_or_count;
        break;
    }

    case ABILITY_ID_REACTION_DISTRIBUTE: {
        s32 unit_id;
        s32 actor_team;

        target_or_count = 0;
        actor_team = actor->team_flags & UNIT_TEAM_MASK;
        for (unit_id = 0; unit_id < UNIT_SLOT_COUNT; unit_id++) {
            if (unit_id == actor_id) {
                continue;
            }
            unit = &unit_base[unit_id];
            if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
                continue;
            }
            if (unit->mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT) {
                continue;
            }
            if (unit->hp == unit->max_hp) {
                continue;
            }
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
                & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD)
                    | STATUS_BYTE_MASK(STATUS_ID_JUMP))) {
                continue;
            }
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
                & (STATUS_BYTE_MASK(STATUS_ID_PETRIFY) | STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
                continue;
            }
            /* Retail keeps the candidate's team in the tile-index stack slot. */
            tile_index = unit->initial_team_flags & UNIT_TEAM_MASK;
            if (tile_index == actor_team) {
                target_ids[target_or_count++] = unit_id;
            }
        }
        g_reaction_distribute_target_count = target_or_count;
        *target_count = target_or_count;
        break;
    }

    case ABILITY_ID_REACTION_DAMAGE_SPLIT:
        unit = &unit_base[g_action_actor_id];
        if (unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE) {
            *target_count = 1;
            *target_ids = g_action_actor_id;
        } else {
            *target_count = 0;
        }
        break;

    default:
        flags = g_ability_reaction_behavior_flags[g_reaction_ability_id - ABILITY_ID_REACTION_FIRST];
        if (flags & REACTION_BEHAVIOR_FLAG_TARGET_SELF) {
            *target_count = 1;
            *target_ids = actor_id;
            break;
        }
        if (flags & REACTION_BEHAVIOR_FLAG_TARGET_ATTACKER) {
            return 0;
        }
        *target_count = 0;
        break;
    }
    return 1;
}

/*
 * Compute target tiles for the action and report its ability flags_3 byte (0
 * for anything but an ordinary or monster ability). Returns -1 when targeting
 * fails.
 *
 * Attack, Charge, item-inventory, weapon-inventory, direct-targeting, and
 * weapon-range actions use the weapon/direct resolver. Random-fire selection
 * runs only after successful ordinary targeting while an action executes.
 */
static s32 action_tiles_calculate(const action_command_t* action, u8* flags_3) {
    ability_secondary_data_t* ability;
    u8 menu_type;
    u8 flags_4 = 0;
    u8 flags_1 = 0;
    u8 flags_2;
    s32 result;
    s16 ability_id;

    menu_type = g_command_menu_types_by_skillset[action->skillset_id];
    flags_2 = 0;
    *flags_3 = 0;
    if (menu_type == ACTION_MENU_TYPE_DEFAULT || menu_type == ACTION_MENU_TYPE_MONSTER) {
        ability_id = action->ability_id;
        if (ability_id < ABILITY_ID_ITEM_FIRST) {
            ability = &g_ability_range_data[ability_id];
            *flags_3 = ability->flags_3;
            flags_4 = ability->flags_4;
            flags_1 = ability->flags_1;
            flags_2 = ability->flags_2;
        }
    }
    if (menu_type == ACTION_MENU_TYPE_ATTACK || menu_type == ACTION_MENU_TYPE_CHARGE
        || menu_type == ACTION_MENU_TYPE_ITEM_INVENTORY || menu_type == ACTION_MENU_TYPE_WEAPON_INVENTORY
        || (flags_4 & ABILITY_SECONDARY_FLAG_4_DIRECT_TARGETING) || (flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE)) {
        result = target_validate_for_weapon(action);
    } else {
        result = target_area_set_for_action(action);
        if (result != -1 && (flags_2 & ABILITY_SECONDARY_FLAG_2_RANDOM_FIRE)
            && g_action_state == ACTION_STATE_EXECUTE) {
            random_fire_tile_select();
        }
    }
    return result;
}

/* Resolve one strike of the current ability: build the target list, run the
 * per-target formula setup and fill the strike work record.
 *
 * Returns -1 for an invalid actor or targeting, 1 when nothing was hit and 0
 * otherwise. After the clearing loop i holds 21, which the target reuses as
 * the Math skillset id in the control-value test. The (u16*)/(s16*) views
 * reproduce the target's load forms and memory-access ordering. */
s32 action_resolve_strike(s32 battle_id, action_strike_work_t* work) {
    u8 targets[UNIT_SLOT_COUNT];
    s32 count;
    u8 flags_3;
    unit_stats_t* attacker;
    s32 i;
    s32 reaction_targeted;
    u8 target_id;
    u16 reaction_id;
    battle_map_tile_t* tile;
    s32 location;

    g_target_sort_nearest_first = 0;
    g_unit_relocated_count = 0;
    g_action_work.random_fire_flag = 0;
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        targets[i] = 0xff;
    }

    if (battle_id >= UNIT_SLOT_COUNT) {
        return -1;
    }

    attacker = &g_unit_stats[battle_id];
    action_clear_current_data(&attacker->action);
    attacker->action.hit = 0;
    attacker->action.miss_type = ACTION_MISS_TYPE_CANCELED;
    action_clear_current_data(&g_action_attacker_action_data);
    g_action_attacker_action_data.hit = 0;
    work->knockback_flags = 0;
    work->post_action_message = ACTION_POST_MESSAGE_NONE;
    work->math_skill_flag = 0;
    work->reaction_ability_id = 0;
    reaction_targeted = 0;

    /* A reaction builds its own target list; Reflect's strike animates even on a miss. */
    if (g_action_context != ACTION_CONTEXT_PRIMARY) {
        work->reaction_ability_id = g_reaction_ability_id;
        if (reaction_targets_build(battle_id, work, &count, targets)) {
            if (g_reaction_ability_id == ABILITY_ID_REACTION_REFLECT) {
                flags_3 = ABILITY_SECONDARY_FLAG_3_ANIMATE_ON_MISS;
            } else {
                flags_3 = 0;
            }
            reaction_targeted = 1;
        }
    } else if (attacker->action_command.skillset_id == i) {
        /* i is 21 after the clearing loop above: the Math skillset id. */
        work->math_skill_flag = 1;
    }

    if (!reaction_targeted) {
        if (action_tiles_calculate(&attacker->action_command, &flags_3) == -1) {
            work->target_count = 0;
            work->continue_attack = 0;
            return -1;
        }
        /* A pending weapon proc or knockback narrows the panels to its post-action
         * target's tile; the list itself is rebuilt from the panels below. */
        if (g_action_work.post_action_target_id < UNIT_SLOT_COUNT
            && (g_action_work.weapon_proc_pending != 0 || (g_action_work.knockback_flags & MOVE_KNOCKBACK_PENDING))) {
            target_area_clear_flags();
            count = 1;
            targets[0] = g_action_work.post_action_target_id;
            for (i = 1; i < UNIT_RUNTIME_SLOT_COUNT; i++) {
                targets[i] = 0xff;
            }
            location = battle_map_tile_calculate_unit_index(&g_unit_stats[g_action_work.post_action_target_id]);
            tile = &g_battle_map_tile_data[location];
            tile->ceiling_depth_and_marks.targeted = 1;
        }
        count = units_on_panels_list(targets, attacker);
    }

    g_action_work.target_count = count;
    action_store_target_ids(targets);
    action_store_used_weapon(&attacker->action_command);
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        target_id = targets[i];
        if (target_id != 0xff) {
            g_action_work.defaulted_to_attack = 0;
            action_calculate_target_result(&attacker->action_command, target_id);
        }
    }

    status_remove_transparent_if_jump_used(attacker);
    count = action_copy_ability_targets(work->target_list);
    work->target_count = count;
    g_action_work.target_count = count;

    if (g_action_context == ACTION_CONTEXT_PRIMARY) {
        g_action_work.can_earn_exp_jp = 1;
    }
    work->can_earn_experience = 0;
    draw_out_katana_finalize(attacker, work, count);
    work->actor_id = battle_id;
    work->used_weapon_id = g_action_work.weapon_id;
    work->reaction_occurred = 0;
    work->current_hit_number = g_action_work.strike_counter;
    strike_coordinates_set(attacker, work);

    if (count == 0) {
        g_action_work.strike_counter++;
        work->last_attack_id = attacker->action_command.ability_id;
        work->reaction_id = 0;
        if (!(flags_3 & ABILITY_SECONDARY_FLAG_3_ANIMATE_ON_MISS)) {
            work->skip_empty_tile_effect = 1;
        }
        if (g_action_work.strike_counter < g_action_work.strike_count) {
            work->continue_attack = 1;
        } else {
            work->continue_attack = 0;
        }
        return 1;
    }

    work->post_action_message = g_action_work.post_action_message;
    work->skip_empty_tile_effect = 0;
    reaction_id = g_action_work.reaction_id;
    if (reaction_id != 0 && g_action_work.weapon_proc_pending == 0) {
        work->last_attack_id = reaction_id;
        work->reaction_occurred = 1;
    } else if (g_action_work.defaulted_to_attack != 0) {
        work->last_attack_id = 0;
    } else {
        work->last_attack_id = attacker->action_command.ability_id;
    }
    work->reaction_id = g_action_work.reaction_id;
    if (g_action_work.knockback_flags != 0) {
        work->knockback_flags = g_action_work.knockback_flags & MOVE_KNOCKBACK_KIND_MASK;
        work->target_tile.x = g_action_work.target_tile.x;
        work->target_tile.y = g_action_work.target_tile.y;
        work->target_tile.layer = g_action_work.target_tile.layer;
    }

    /* A pending weapon proc or knockback continues the attack without counting a strike. */
    if (g_action_work.weapon_proc_pending != 0 || (g_action_work.knockback_flags & MOVE_KNOCKBACK_PENDING)) {
        work->continue_attack = 1;
    } else {
        if (g_action_work.knockback_flags != 0) {
            work->reaction_occurred = 1;
            work->skip_empty_tile_effect = 1;
            work->last_attack_id = ABILITY_ID_KNOCKBACK;
        }
        g_action_work.strike_counter++;
        if (g_action_work.strike_counter < g_action_work.strike_count) {
            work->continue_attack = 1;
        } else {
            work->continue_attack = 0;
        }
    }
    return 0;
}

/* Store the tile a strike lands on in the strike work record: the random-fire
 * tile picked for this strike, else the target unit's tile and level, else the
 * action's target tile. skip_empty_tile_effect is cleared. */
static void strike_coordinates_set(const unit_stats_t* acting, action_strike_work_t* out) {
    unit_stats_t* target;

    out->skip_empty_tile_effect = 0;
    if (g_action_work.random_fire_flag != 0) {
        out->target_tile.x = g_action_work.target_tile.x;
        out->target_tile.y = g_action_work.target_tile.y;
        out->target_tile.layer = g_action_work.target_tile.layer;
    } else if (acting->action_command.targeting_type == TARGET_TYPE_UNIT) {
        target = &g_unit_stats[acting->action_command.target_id];
        out->target_tile.x = target->x;
        out->target_tile.y = target->position.y;
        out->target_tile.layer = target->position.higher_elevation;
    } else {
        out->target_tile.x = acting->action_command.target_position.vx;
        out->target_tile.y = acting->action_command.target_position.vz;
        out->target_tile.layer = acting->action_command.target_position.vy;
    }
}

/* Finalize Draw Out's katana-consumption result after target processing.
 *
 * hit_count is the number of units the strike hit; with none, or when the
 * katana did not break, the attacker's own result becomes a pseudo-status hit
 * marked DRAW_OUT_KATANA_NOT_BROKEN, which gives the spent katana back when it
 * is applied. A Mime's Draw Out is left alone. When the katana broke, retail
 * copies can_earn_experience onto itself (lbu/sb of the same byte): a genuine
 * no-op kept for the match. */
static void draw_out_katana_finalize(unit_stats_t* attacker, action_strike_work_t* work, u8 hit_count) {
    u16 flags;

    if (g_command_menu_types_by_skillset[attacker->action_command.skillset_id] != ACTION_MENU_TYPE_KATANA_INVENTORY)
        return;
    if (attacker->job_id == JOB_ID_MIME)
        return;
    if (hit_count == 0
        || ((flags = g_action_attacker_action_data.special_effect) & ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_NOT_BROKEN)
            != 0) {
        g_action_attacker_action_data.special_effect = ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_NOT_BROKEN;
        g_action_attacker_action_data.attack_type = ACTION_TYPE_PSEUDO_STATUS;
        g_action_attacker_action_data.hit = 1;
        return;
    }
    if (flags & ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_BROKEN) {
        work->can_earn_experience = work->can_earn_experience;
    }
}

/*
 * Seed the strike state before an action resolves: reset the strike counter,
 * proc and knockback scratch, clear the EXP/JP flags in the primary action
 * context, and latch the two hands (the left moves up when the right is empty).
 * Throw only records the thrown item. ABILITY_FORMULA_MULTIHIT_TRUTH_MAGIC and
 * ABILITY_FORMULA_UN_TRUTH strike a random 1..X times while
 * executing (once in a preview) and ABILITY_FORMULA_FIXED_MULTIHIT_MAGIC
 * strikes X+1 times; Attack, Charge and ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE
 * abilities strike twice with Two Swords when both hands hold weapons or both
 * are empty (Frog and monsters count as empty).
 *
 * The `ability_id >= 0 || ability_id < ABILITY_ID_ITEM_FIRST` guard is the target's own: the
 * second arm is unreachable, so the table read is never actually skipped.
 *
 * `left_hand` is load-bearing: the target issues the equipment[5] reload
 * BEFORE the 0xff store, which costs it the menu-type zero-extension in $v1
 * and makes it re-emit `andi v1,a2,0xff` after the block. Reading the slot
 * straight into the global instead sinks the load past the store and drops
 * that instruction.
 */
static void strike_data_init(unit_stats_t* unit) {
    ability_secondary_data_t* data;
    u8 menu_type;
    u8 formula;
    u8 x;
    s16 ability_id;
    u8 support;
    u8 left_hand;

    g_action_work.strike_count = 1;
    g_action_work.strike_counter = 0;
    g_action_work.weapon_proc_pending = 0;
    g_action_work.knockback_flags = 0;
    formula = 0;
    if (g_action_context == ACTION_CONTEXT_PRIMARY) {
        g_action_work.can_earn_exp_jp = 0;
        g_action_work.earned_experience = 0;
        g_action_work.level_gained_flag = 0;
        g_action_work.job_level_gained_flag = 0;
    }

    menu_type = g_command_menu_types_by_skillset[unit->action_command.skillset_id];
    g_action_loaded_ability_flags_1 = 0;
    g_action_work.primary_weapon_id = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
    g_action_work.secondary_weapon_id = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
    if (g_action_work.primary_weapon_id == ITEM_ID_NONE) {
        left_hand = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
        g_action_work.secondary_weapon_id = ITEM_ID_NONE;
        g_action_work.primary_weapon_id = left_hand;
    }

    if (menu_type == ACTION_MENU_TYPE_WEAPON_INVENTORY) {
        g_action_work.primary_weapon_id = unit->action_command.item_id;
        return;
    }

    if (menu_type == ACTION_MENU_TYPE_DEFAULT || menu_type == ACTION_MENU_TYPE_MONSTER) {
        ability_id = unit->action_command.ability_id;
        if (ability_id >= 0 || ability_id < ABILITY_ID_ITEM_FIRST) {
            data = &g_ability_range_data[ability_id];
            g_action_loaded_ability_flags_1 = data->flags_1;
            x = data->x;
            formula = data->formula;
        }
        if (formula == ABILITY_FORMULA_MULTIHIT_TRUTH_MAGIC || formula == ABILITY_FORMULA_UN_TRUTH) {
            if (g_action_state == ACTION_STATE_EXECUTE) {
                g_action_work.strike_count = ((x * rand()) / 32768) + 1;
                return;
            }
        }
        if (formula == ABILITY_FORMULA_FIXED_MULTIHIT_MAGIC) {
            g_action_work.strike_count = x + 1;
            return;
        }
    }

    if (menu_type == ACTION_MENU_TYPE_JUMP) {
        return;
    }
    if (menu_type != ACTION_MENU_TYPE_ATTACK && menu_type != ACTION_MENU_TYPE_CHARGE) {
        if ((g_action_loaded_ability_flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE) == 0) {
            return;
        }
    }

    support = unit->ability_flags.support[2];
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG))
        || (unit->unit_flags & UNIT_FLAG_MONSTER)) {
        g_action_work.primary_weapon_id = ITEM_ID_NONE;
        g_action_work.secondary_weapon_id = ITEM_ID_NONE;
    }

    if ((g_action_work.primary_weapon_id == ITEM_ID_NONE && g_action_work.secondary_weapon_id == ITEM_ID_NONE)
        || (g_action_work.primary_weapon_id != ITEM_ID_NONE && g_action_work.secondary_weapon_id != ITEM_ID_NONE)) {
        if (support & UNIT_SUPPORT_SET_3_TWO_SWORDS) {
            g_action_work.strike_count = 2;
        }
    }
}

/*
 * Pick a random tile among those whose BATTLE_MAP_TILE_FLAG_TARGETED is set,
 * clearing the flag on every candidate and setting it again on the chosen
 * tile, then store that tile as the current action target.
 *
 * The candidate list reuses the targeting panels' mark byte for the tile
 * index, so upper-level candidates are recognized by their list position.
 */
static void random_fire_tile_select(void) {
    s32 count = 0;
    s32 lower_count;
    s32 i;
    s32 index;
    s32 tile;
    battle_map_tile_marks_t marks;
    battle_map_tile_t* tile_data;
    target_panel_t* panel;

    for (i = 0; i < BATTLE_MAP_TILE_COUNT; i++) {
        tile_data = &g_battle_map_tile_data[i];
        /* The upper layer's tiles start at index 0x100. */
        if (i == 0x100) {
            lower_count = count;
        }
        marks = tile_data->ceiling_depth_and_marks;
        if (marks.targeted) {
            tile_data->ceiling_depth_and_marks.targeted = 0;
            panel = &g_target_panels[count];
            count++;
            panel->mark = i;
        }
    }
    index = rand() * count / 32768;
    tile = g_target_panels[index].mark;
    if (index >= lower_count) {
        tile += 0x100;
    }
    g_battle_map_tile_data[tile].ceiling_depth_and_marks.targeted = 1;
    g_action_work.random_fire_flag = 1;
    /* A tile index is level * 256 + y * map width + x, and g_battle_map_terrain_width is the width. */
    g_action_work.target_tile.layer = tile / 256;
    g_action_work.target_tile.y = (tile & 0xFF) / g_battle_map_terrain_width;
    g_action_work.target_tile.x = (tile & 0xFF) % g_battle_map_terrain_width;
    target_clear_panel_spread_flags();
}

/* Fill `list` (UNIT_SLOT_COUNT bytes, 0xff padded) with the ids of present units whose
 * tile carries `BATTLE_MAP_TILE_FLAG_TARGETED`, and return the count.
 * target_is_unit_untargetable_and_store_tile returns nonzero for units
 * that cannot be counted and writes the unit's tile index. When nearest-first
 * ordering is enabled the list is insertion-sorted by Manhattan distance from
 * `origin`.
 */
static s32 units_on_panels_list(u8* list, const unit_stats_t* origin) {
    s32 distance[UNIT_SLOT_COUNT];
    s32 tile;
    s32 count;
    s32 i;
    s32 j;
    s32 k;
    s32 dx;
    s32 dy;
    s32 dist;
    s32 x;
    s32 y;
    unit_stats_t* unit;

    count = 0;
    x = origin->x;
    y = origin->position.y;
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        unit = &g_unit_stats[i];
        /* A byte flag test retains the original compound-condition scheduling. */
        if (unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE
            && target_is_unit_untargetable_and_store_tile(i, &tile) == 0
            && ((*(u8*)&g_battle_map_tile_data[tile].ceiling_depth_and_marks) & BATTLE_MAP_TILE_FLAG_TARGETED)) {
            if (g_target_sort_nearest_first != 0) {
                dx = x - unit->x;
                if (dx < 0) {
                    dx = -dx;
                }
                dy = y - unit->position.y;
                if (dy < 0) {
                    dy = -dy;
                }
                dist = dx + dy;
                for (j = 0; j < count; j++) {
                    if (dist < distance[j]) {
                        break;
                    }
                }
                for (k = UNIT_SLOT_COUNT - 1; j < k; k--) {
                    list[k] = list[k - 1];
                    distance[k] = distance[k - 1];
                }
                distance[j] = dist;
            } else {
                j = count;
            }
            list[j] = i;
            count++;
        }
    }
    for (j = count; j < UNIT_SLOT_COUNT; j++) {
        list[j] = 0xFF;
    }
    return count;
}

/*
 * Check action restrictions and consume MP outside preview mode.
 *
 * Return 0 when allowed, or an action_rejection_e: an action restriction (Don't
 * Act, a ridden mount, deep water), a Frog using anything but Attack, Frog
 * Attack or the Frog spell (or any Charge), Silence against a silenceable
 * ability, or too little MP. Only ordinary-menu, non-Mime abilities consume MP,
 * halved and rounded down by Half of MP; the effective cost is recorded first.
 * Preview skips only subtraction; earlier bookkeeping still runs. In the primary
 * context the unit's Charging, Jump and Defending end first (Performing stays).
 */
static s32 mp_check_and_consume(unit_stats_t* unit) {
    s16 ability_id;
    u8 menu;
    s32 result;
    u8 cost;
    s16 mp;
    ability_secondary_data_t* ability;

    ability_id = unit->action_command.ability_id;
    menu = g_command_menu_types_by_skillset[unit->action_command.skillset_id];
    g_action_work.mp_cost = 0;
    if (g_action_context == ACTION_CONTEXT_PRIMARY)
        status_action_state_set(unit, STATUS_ACTION_STATE_KEEP_PERFORMING);
    result = unit_get_action_restriction(unit);
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_ACT)] & STATUS_BYTE_MASK(STATUS_ID_DONT_ACT))
        result = UNIT_ACTION_RESTRICTION_CANNOT_ACT;
    if (result)
        return result + ACTION_REJECTION_RESTRICTION_OFFSET;
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG)) {
        if (ability_id != 0 && ability_id != ABILITY_ID_FROG_ATTACK && ability_id != ABILITY_ID_BLACK_MAGIC_FROG)
            return ACTION_REJECTION_FROG;
        if (menu == ACTION_MENU_TYPE_CHARGE)
            return ACTION_REJECTION_FROG;
    }
    if (ability_id >= 0 && ability_id < ABILITY_ID_ITEM_FIRST) {
        ability = &g_ability_range_data[ability_id];
        if ((menu == ACTION_MENU_TYPE_DEFAULT || menu == ACTION_MENU_TYPE_MONSTER)
            && (ability->flags_3 & ABILITY_SECONDARY_FLAG_3_AFFECTED_BY_SILENCE)
            && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SILENCE)] & STATUS_BYTE_MASK(STATUS_ID_SILENCE)))
            return ACTION_REJECTION_SILENCED;
        if (menu == ACTION_MENU_TYPE_DEFAULT && unit->job_id != JOB_ID_MIME) {
            cost = ability->mp_cost;
            if (unit->ability_flags.support[1] & UNIT_SUPPORT_SET_2_HALF_MP)
                cost >>= 1;
            g_action_work.mp_cost = cost;
            /* A signed member view retains the target store-before-load order;
             * casting the unsigned value hoists the MP load and loses a nop. */
            mp = unit->mp;
            if (mp < cost)
                return ACTION_REJECTION_NOT_ENOUGH_MP;
            if (g_action_state != ACTION_STATE_PREVIEW)
                unit->mp = mp - cost;
        }
    }
    return 0;
}

/*
 * Save the actor's position or restore its pre-reaction action command.
 *
 * A Mime drops the weapons it borrowed for the copied action; any other unit
 * acting in the primary context records its facing for Mimes. Normal actions
 * save X/Y and elevation; reactions restore the action command saved in
 * g_action_saved_command and return to the primary context.
 */
void action_store_actor_data(unit_stats_t* unit) {
    if (unit->job_id == JOB_ID_MIME) {
        unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = ITEM_ID_NONE;
        unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = ITEM_ID_NONE;
    } else if (g_action_context == ACTION_CONTEXT_PRIMARY) {
        g_action_mimic_attacker.facing = unit->position.facing;
    }

    if (g_action_context == ACTION_CONTEXT_PRIMARY) {
        g_action_actor_x = unit->x;
        g_action_actor_y = unit->position.y;
        s_action_actor_elevation = unit->position.higher_elevation;
    } else {
        action_command_copy(&g_action_saved_command, &unit->action_command);
        g_action_context = ACTION_CONTEXT_PRIMARY;
    }
}

/* The unit on a tile: unit_find_at_tile with no status filter. */
s32 target_find_unit_at_tile(s32 x, s32 y, s32 level) {
    return unit_find_at_tile(x, y, level, 0);
}

/*
 * Find an eligible unit occupying the requested map tile.
 *
 * The status filter bits are alternatives: a unit with any requested status
 * is eligible. The Chocobo bit is an additional requirement. Mounted riders
 * resolve to the mount's battle slot. Returns the unit's slot, -1 for an empty
 * tile, or -2 for coordinates off the map, a blocked tile, or (with
 * UNIT_TILE_FILTER_DRY_LOW_SLOPE) water or a slope of 3 or more.
 */
s32 unit_find_at_tile(s32 x, s32 y, u32 level, s32 filters) {
    s32 unit_index;
    unit_stats_t* unit;
    battle_map_tile_t* tile;
    u8 mount_info;
    /* The unit loop tests a byte copy of the filters. */
    u8 unit_filters;

    unit_filters = filters;

    if (x < 0 || x >= g_battle_map_terrain_width || y < 0 || y >= g_battle_map_terrain_depth || level >= 2) {
        return -2;
    }

    tile = &g_battle_map_tile_data[(level << 8) + y * g_battle_map_terrain_width + x];
    /* A byte collision test retains the original single-mask branch. */
    if (((*(u8*)&tile->flags) & BATTLE_MAP_TILE_COLLISION_MASK)) {
        return -2;
    }
    if (filters & UNIT_TILE_FILTER_DRY_LOW_SLOPE) {
        if (tile->depth_half_height.depth != 0) {
            return -2;
        }
        if (tile->depth_half_height.half_height >= 3) {
            return -2;
        }
    }

    for (unit_index = 0; unit_index < UNIT_SLOT_COUNT; unit_index++) {
        unit = &g_unit_stats[unit_index];
        if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
            continue;
        }
        if ((unit_filters & UNIT_TILE_STATUS_FILTERS)
            && !((unit_filters & UNIT_TILE_FILTER_CRYSTAL)
                && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
                    & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL)))
            && !((unit_filters & UNIT_TILE_FILTER_TREASURE)
                && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)]
                    & STATUS_BYTE_MASK(STATUS_ID_TREASURE)))
            && !((unit_filters & UNIT_TILE_FILTER_TRANSPARENT)
                && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
                    & STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT)))) {
            continue;
        }
        if ((unit_filters & UNIT_TILE_FILTER_MOUNTABLE_CHOCOBO)
            && unit_mount_get_chocobo_state(unit) == UNIT_CHOCOBO_MOUNT_NONE) {
            continue;
        }
        if (unit->x != x || unit->position.y != y || unit->position.higher_elevation != level) {
            continue;
        }

        mount_info = unit->mount_info;
        if (mount_info >= UNIT_MOUNT_INFO_FLAG_RIDER) {
            return mount_info & UNIT_MOUNT_INFO_PARTNER_ID_MASK;
        }
        return unit_index;
    }

    return -1;
}

/* Clear the range and the mark of all 512 targeting panels. */
void target_clear_panels(void) {
    s32 i = 0;
    target_panel_t* panel = g_target_panels;

    for (; i < BATTLE_MAP_TILE_COUNT; i++) {
        panel->remaining_range = 0;
        panel->mark = 0;
        panel++;
    }
}

/*
 * Give the single unit targeted by `id`'s pending action a chance to Hamedo
 * the actor, reporting the reaction ability through `out_ability`.
 *
 * Only an Attack, Charge, Throw or weapon-range action that the actor can pay
 * for (its MP is put back after the check) and that reaches exactly one other
 * unit qualifies; that unit needs Hamedo, a passed Brave roll, no status that
 * prevents reactions, and a counterattack it can aim at the actor. On success
 * the counter is staged in the reaction context and g_reaction_hamedo_flag
 * is set, which makes the actor's own action fail; a Mime actor drops its
 * borrowed weapons. Returns the reacting unit's slot, or -1 when nothing reacts.
 *
 * target_validate_for_weapon takes only the action record (it copies the record and
 * rederives the menu type itself); menu_type and flags_1 merely happen to sit
 * in $a1/$a2 at that call. Passing them as arguments would set $a2 twice and
 * change the scheduling of the counter-ability call's argument setup.
 */
s32 reaction_prepare_hamedo(s32 id, s16* out_ability) {
    u8 targets[UNIT_SLOT_COUNT];
    unit_stats_t* unit;
    unit_stats_t* base;
    unit_stats_t* target;
    action_command_t* action;
    s32 i;
    s32 count;
    s32 result;
    u8 menu_type;
    u8 flags_1;
    u16 mp;
    s32 target_id;
    u16 ability_id;

    unit = &g_unit_stats[id];
    base = g_unit_stats;
    g_reaction_hamedo_flag = 0;
    if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
        return -1;
    }
    mp = unit->mp;
    result = mp_check_and_consume(unit);
    unit->mp = mp;
    if (result != 0) {
        return -1;
    }

    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        base[i].action.reaction_id = 0;
        base[i].ability_outcome = 0;
    }

    menu_type = g_command_menu_types_by_skillset[unit->action_command.skillset_id];
    flags_1 = 0;
    if (menu_type == ACTION_MENU_TYPE_DEFAULT || menu_type == ACTION_MENU_TYPE_MONSTER) {
        ability_id = unit->action_command.ability_id;
        if (ability_id < ABILITY_ID_ITEM_FIRST) {
            flags_1 = g_ability_range_data[ability_id].flags_1;
        }
    }

    result = -1;
    if (menu_type == ACTION_MENU_TYPE_ATTACK || menu_type == ACTION_MENU_TYPE_CHARGE
        || menu_type == ACTION_MENU_TYPE_WEAPON_INVENTORY || (flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE)) {
        result = target_validate_for_weapon(&unit->action_command);
    }
    if (result == -1) {
        return -1;
    }

    g_target_sort_nearest_first = 0;
    count = units_on_panels_list(targets, unit);
    if (count != 1) {
        return -1;
    }
    target_id = targets[0];
    if (target_id == id) {
        return -1;
    }
    target = &g_unit_stats[target_id];
    if (!(target->ability_flags.reaction[3] & UNIT_REACTION_SET_4_HAMEDO)) {
        return -1;
    }
    if (reaction_fails_brave_roll(target) != 0) {
        return -1;
    }
    action = &target->action_command;
    if (reaction_is_prevented(target) != 0) {
        return -1;
    }

    g_action_context = ACTION_CONTEXT_SECONDARY;
    action_command_copy(action, &g_action_saved_command);
    g_reaction_ability_id = ABILITY_ID_REACTION_HAMEDO;
    g_action_actor_id = id;
    target->action_command.actor_id = target_id;
    if (counter_ability_store(target, SKILLSET_ID_ATTACK, 0, 1) == 0 && mp_check_and_consume(target) == 0) {
        strike_data_init(target);
        *out_ability = g_reaction_ability_id;
        g_reaction_hamedo_flag = 1;
        if (unit->job_id == JOB_ID_MIME) {
            unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = ITEM_ID_NONE;
            unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = ITEM_ID_NONE;
        }
        return target_id;
    }
    action_command_copy(&g_action_saved_command, action);
    return -1;
}

/* Reaction-ability outcome carried in `outcome` between the per-ability arms
 * and the common tail. ATTACK and COUNTER pay MP; ATTACK through REFLECT get the
 * full strike setup; SELF_TILE aims a strike at the reactor's own tile (the
 * attacker's for Damage Split). */
enum {
    REACTION_OUTCOME_SELF_TILE = -1,
    REACTION_OUTCOME_NONE = 0,
    REACTION_OUTCOME_ATTACK = 1, /* the default: a counterattack */
    REACTION_OUTCOME_COUNTER = 2,
    REACTION_OUTCOME_REFLECT = 3
};

/* Find the first unit holding a pending reaction and prepare it.
 *
 * Walks the 21 unit slots, skipping the acting unit, and for the first unit
 * whose queued reaction id is set and who can still react, saves the unit's
 * action block, rebuilds it for the reaction and runs the matching preparation.
 * Returns the reacting unit's slot and writes its reaction ability id to
 * `out_ability`, or -1 when no unit reacts.
 *
 * The action pointer has its own register because stores use that base while
 * the unit pointer can change for Damage Split.
 */
s32 reaction_prepare_next(s16* out_ability) {
    unit_stats_t* unit;
    action_command_t* action;
    s32 i;
    s32 outcome;
    s32 result;
    s32 reaction_prevented;
    u16 ability;
    u16 last_attack;
    s16 reaction_id;
    s32 surface;

    if (g_action_actor_id == -1) {
        return -1;
    }

    i = 0;
    do {
        g_action_context = ACTION_CONTEXT_PRIMARY;
        if (i != g_action_actor_id) {
            unit = &g_unit_stats[i];
            action = &unit->action_command;
            if (unit_get_action_restriction(unit) == UNIT_ACTION_RESTRICTION_NONE
                && unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE) {
                reaction_prevented = reaction_is_prevented(unit);
                ability = unit->action.reaction_id;
                /* Reflect fires even when reactions are prevented and during AI simulation. */
                if (((reaction_prevented == 0 && g_action_state != ACTION_STATE_AI_SIMULATION)
                        || ability == ABILITY_ID_REACTION_REFLECT)
                    && ability != 0) {
                    g_action_context = ACTION_CONTEXT_SECONDARY;
                    action_command_copy(action, &g_action_saved_command);
                    g_reaction_ability_id = ability;
                    action->actor_id = i;
                    action->skillset_id = 0;
                    reaction_id = g_reaction_ability_id;
                    outcome = REACTION_OUTCOME_ATTACK;
                    action->ability_id = reaction_id;
                    last_attack = unit->action.last_received_attack;
                    unit->action.reaction_id = 0;

                    switch (reaction_id) {
                    case ABILITY_ID_REACTION_REFLECT:
                        outcome = reflect_apply(unit) == 0 ? REACTION_OUTCOME_REFLECT : REACTION_OUTCOME_NONE;
                        break;
                    case ABILITY_ID_REACTION_MEATBONE_SLASH:
                        if (counter_ability_store(unit, SKILLSET_ID_ATTACK, 0, 1) != 0) {
                            outcome = REACTION_OUTCOME_NONE;
                        } else {
                            action->skillset_id = SKILLSET_ID_ATTACK;
                            action->targeting_type = TARGET_TYPE_TILE;
                        }
                        break;
                    case ABILITY_ID_REACTION_COUNTER_MAGIC:
                        outcome = counter_ability_store(unit, SKILLSET_ID_BLACK_MAGIC, last_attack, 0) == 0
                            ? REACTION_OUTCOME_COUNTER
                            : REACTION_OUTCOME_NONE;
                        break;
                    case ABILITY_ID_REACTION_COUNTER_TACKLE:
                        if (counter_ability_store(unit, SKILLSET_ID_BLACK_MAGIC, ABILITY_ID_BASIC_SKILL_DASH, 1) != 0) {
                            outcome = REACTION_OUTCOME_NONE;
                        }
                        break;
                    case ABILITY_ID_REACTION_COUNTER_FLOOD:
                        surface = g_battle_map_tile_data[battle_map_tile_calculate_unit_index(unit)].surface.type;
                        outcome = counter_ability_store(
                                      unit, SKILLSET_ID_ELEMENTAL, g_terrain_geomancy_abilities[surface], 0)
                                == 0
                            ? REACTION_OUTCOME_COUNTER
                            : REACTION_OUTCOME_NONE;
                        break;
                    case ABILITY_ID_REACTION_AUTO_POTION:
                        result = auto_potion_item_select(unit);
                        if (result == -1) {
                            outcome = REACTION_OUTCOME_NONE;
                            break;
                        }
                        action->skillset_id = SKILLSET_ID_ITEM;
                        action->item_id = result;
                        action->targeting_type = TARGET_TYPE_TILE;
                        action->target_id = i;
                        action->target_position.vx = unit->x;
                        action->target_position.vz = unit->position.y;
                        action->target_position.vy = unit->position.higher_elevation;
                        result = action_commit_preparation(action);
                        outcome = REACTION_OUTCOME_COUNTER;
                        unit->action_command.ability_id = ABILITY_ID_REACTION_AUTO_POTION;
                        if (result == ACTION_PREPARATION_INVALID || result == ACTION_PREPARATION_CHANGE_EQUIPMENT
                            || result == ACTION_PREPARATION_JUMP) {
                            outcome = REACTION_OUTCOME_NONE;
                        }
                        break;
                    case ABILITY_ID_REACTION_COUNTER:
                        if (counter_ability_store(unit, SKILLSET_ID_ATTACK, 0, 1) != 0) {
                            outcome = REACTION_OUTCOME_NONE;
                        }
                        break;
                    default:
                        outcome = (g_ability_reaction_behavior_flags[g_reaction_ability_id - ABILITY_ID_REACTION_FIRST]
                                      & (REACTION_BEHAVIOR_FLAG_PASSIVE | REACTION_BEHAVIOR_FLAG_ACTIVE))
                                == 0
                            ? REACTION_OUTCOME_SELF_TILE
                            : REACTION_OUTCOME_NONE;
                        break;
                    }

                    if ((outcome == REACTION_OUTCOME_ATTACK || outcome == REACTION_OUTCOME_COUNTER)
                        && mp_check_and_consume(unit) != 0) {
                        outcome = REACTION_OUTCOME_NONE;
                    }
                    if (outcome == REACTION_OUTCOME_NONE) {
                        action_command_copy(&g_action_saved_command, action);
                        i++;
                        continue;
                    }

                    if (outcome >= REACTION_OUTCOME_ATTACK && outcome <= REACTION_OUTCOME_REFLECT) {
                        strike_data_init(unit);
                    } else {
                        g_action_work.strike_count = 1;
                        g_action_work.strike_counter = 0;
                        g_action_work.weapon_proc_pending = 0;
                        g_action_work.knockback_flags = 0;
                        g_action_work.primary_weapon_id = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
                        g_action_work.secondary_weapon_id = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
                        action->skillset_id = 0;
                        action->targeting_type = TARGET_TYPE_TILE;
                        action->target_id = i;
                        if (g_reaction_ability_id == ABILITY_ID_REACTION_DAMAGE_SPLIT) {
                            action->target_id = g_action_actor_id;
                            unit = &g_unit_stats[g_action_actor_id];
                        }
                        action->target_position.vx = unit->x;
                        action->target_position.vz = unit->position.y;
                        action->target_position.vy = unit->position.higher_elevation;
                    }

                    *out_ability = g_reaction_ability_id;
                    action->actor_id = i;
                    return i;
                }
            }
        }
        i++;
    } while (i < UNIT_SLOT_COUNT);

    g_action_context = ACTION_CONTEXT_PRIMARY;
    *out_ability = 0;
    return -1;
}

/* Aim a reflected spell: the unit casts the ability it received, under the
 * Black Magic skillset, at the tile as far beyond it as the caster is in front
 * of it. -1 when the caster stands on the unit's tile or that tile is off the
 * map or blocked. */
static s32 reflect_apply(unit_stats_t* unit) {
    action_command_t* action = &unit->action_command;
    s32 x;
    s32 y;
    s32 layer;
    s32 dx;
    s32 dy;
    s32 last_attack;

    dx = unit->x - g_action_actor_x;
    dy = unit->position.y - g_action_actor_y;
    if (dx == 0 && dy == 0) {
        return -1;
    }
    if (target_resolve_rotated_offset_layer(unit, dx, dy, 0, &x, &y, &layer) != 0) {
        return -1;
    }
    action->skillset_id = SKILLSET_ID_BLACK_MAGIC;
    last_attack = unit->action.last_received_attack;
    action->targeting_type = TARGET_TYPE_TILE;
    action->target_id = 0;
    action->ability_id = last_attack;
    action->target_position.vx = x;
    action->target_position.vz = y;
    action->target_position.vy = layer;
    return 0;
}

/* Write skillset_id and ability_id into the reacting unit's action command,
 * aimed at the acting unit's tile, and commit it with
 * action_commit_preparation; the unit's own charge CT is kept.
 * With validate_target the ability's range panels are built first and must
 * cover the acting unit's tile. Returns -1 when the panels cannot be built,
 * -2 when the acting unit's tile is out of range and -3 when the preparation
 * fails or yields an equipment change or a Jump; 0 otherwise. */
static s32 counter_ability_store(unit_stats_t* unit, s8 skillset_id, s16 ability_id, s32 validate_target) {
    action_command_t* action;
    unit_stats_t* acting_unit;
    s32 result;
    s32 acting_unit_id;
    s32 preview_result;
    u8 saved_charged_ability_ct;

    acting_unit_id = g_action_actor_id;
    action = &unit->action_command;
    unit->action_command.skillset_id = skillset_id;
    unit->action_command.ability_id = ability_id;
    acting_unit = &g_unit_stats[acting_unit_id];
    if (validate_target != 0) {
        result = target_range_set_for_action(action);
        if (result == TARGET_PANELS_INVALID || result == TARGET_PANELS_NO_TARGETING || result == TARGET_PANELS_NONE) {
            return -1;
        }
        /* A byte flag test retains the original compound-condition scheduling. */
        if (((*(u8*)&g_battle_map_tile_data[battle_map_tile_calculate_unit_index(acting_unit)].ceiling_depth_and_marks)
                & BATTLE_MAP_TILE_FLAG_ABILITY_RANGE)
            == 0) {
            return -2;
        }
    }
    action->targeting_type = TARGET_TYPE_TILE;
    action->target_id = g_action_actor_id;
    action->target_position.vx = acting_unit->x;
    action->target_position.vz = acting_unit->position.y;
    action->target_position.vy = acting_unit->position.higher_elevation;
    saved_charged_ability_ct = unit->charged_ability_ct;
    result = action_commit_preparation(action);
    unit->charged_ability_ct = saved_charged_ability_ct;
    if (result == ACTION_PREPARATION_INVALID
        || (preview_result = (result == ACTION_PREPARATION_CHANGE_EQUIPMENT || result == ACTION_PREPARATION_JUMP),
            preview_result != 0)) {
        preview_result = -3;
    }
    return preview_result;
}

/* The item Auto Potion uses: the first of Potion, Hi-Potion and X-Potion the
 * party has in stock, or -1 with none. A unit that did not start on the blue team
 * always uses a Potion, without spending it. */
static s32 auto_potion_item_select(unit_stats_t* unit) {
    if (unit->initial_team_flags & UNIT_TEAM_MASK) {
        return ITEM_ID_POTION;
    }
    if (g_item_quantities[ITEM_ID_POTION] != 0) {
        return ITEM_ID_POTION;
    }
    if (g_item_quantities[ITEM_ID_HI_POTION] != 0) {
        return ITEM_ID_HI_POTION;
    }
    if (g_item_quantities[ITEM_ID_X_POTION] != 0) {
        return ITEM_ID_X_POTION;
    }
    return -1;
}

/* Award the current action's EXP and JP to a unit and fill display with the gains and any new
 * level or job level. EXP is capped at 255 and may level the unit up. Non-monsters earn JP = 8 + 2
 * * JobLv + Lv/4 (Lv before this action's level-up; none when the action earned no EXP), x3/2 with
 * Gained JP Up; other units whose original team matches the actor's current team get JP/4 (before
 * the bonus) in the actor's job. Returns -1 when the unit cannot earn (Hamedo, absent, Crystal,
 * Dead, Petrify, Treasure). */
s32 reward_apply_action(s32 unit_index, action_reward_display_t* display) {
    s32 job_slot;
    unit_stats_t* unit;
    u8 old_level;
    s32 experience;
    s32 jp;
    s32 unit_id;
    u8 team;
    u8 job_id;
    s32 old_job_level;
    s32 new_job_level;
    s32 job_component;
    s32 level_component;

    display->earned_experience = 0;
    display->earned_jp = 0;
    display->level_for_display = 0;
    display->job_level_for_display = 0;

    if (g_reaction_hamedo_flag != 0) {
        return -1;
    }
    if (g_action_work.can_earn_exp_jp == 0) {
        return 0;
    }

    unit = &g_unit_stats[unit_index];
    if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
        return -1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
        & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD))) {
        return -1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
        & (STATUS_BYTE_MASK(STATUS_ID_PETRIFY) | STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
        return -1;
    }

    experience = unit->experience + g_action_work.earned_experience;
    old_level = unit->level;
    if (experience >= 0x100) {
        experience = 0xff;
    }
    unit->experience = experience;
    if (unit_level_up_check(unit) != 0 || g_action_work.level_gained_flag != 0) {
        display->level_for_display = unit->level;
    }
    display->earned_experience = g_action_work.earned_experience;

    if (unit->unit_flags & UNIT_FLAG_MONSTER) {
        return 0;
    }

    job_id = unit->job_id;
    old_job_level = reward_load_job_level(unit, job_id, &job_slot);
    job_component = old_job_level * 2;
    level_component = (old_level >> 2) + 8;
    jp = job_component + level_component;
    if (g_action_work.earned_experience == 0) {
        jp = 0;
    }

    team = unit->team_flags & UNIT_TEAM_MASK;
    for (unit_id = 0; unit_id < UNIT_SLOT_COUNT; unit_id++) {
        if (unit_id != unit_index) {
            reward_add_job_jp(unit_id, jp / 4, team, job_slot);
        }
    }

    if (unit->ability_flags.support[1] & UNIT_SUPPORT_SET_2_GAINED_JP_UP) {
        jp = jp * 3 / 2;
    }
    team = unit->initial_team_flags & UNIT_TEAM_MASK;
    reward_add_job_jp(unit_index, jp, team, job_slot);

    new_job_level = reward_load_job_level(unit, job_id, &job_slot);
    if (new_job_level != old_job_level) {
        display->job_level_for_display = new_job_level;
    }
    display->earned_jp = jp;
    return 0;
}

/*
 * Return the unit's level (0..8) in the generic job job_id, from the
 * nibble-packed unit_stats_t.career.job_levels bytes (high nibble first).
 * *out_slot receives the zero-based job slot (Chemist = 1 ... Mime = 19),
 * or 0 for Squire and non-generic jobs.
 */
s32 reward_load_job_level(unit_stats_t* unit, s32 job_id, s32* out_slot) {
    s32 slot;
    s32 packed;

    if (job_id >= JOB_ID_CHEMIST && job_id <= JOB_ID_MIME) {
        *out_slot = job_id - JOB_ID_SQUIRE;
    } else {
        *out_slot = 0;
    }
    slot = *out_slot;
    packed = unit->career.job_levels[slot / 2];
    /* Reassigning packed in both arms keeps the target's `sra`; returning
     * `packed >> 4` directly lets GCC use the byte's range and emit `srl`. */
    if (!(slot & 1)) {
        packed >>= 4;
    } else {
        packed &= 0xF;
    }
    return packed;
}

/* Add JP to a battle unit's job slot (both its current and total JP, each
 * capped at UNIT_JOB_POINT_MAX) and recompute its job level and unlocks.
 *
 * Skips absent, crystal, dead, treasure, monster and other-team units, and
 * the Bard slot for females and the Dancer slot for males. The battle twin of
 * job_proposition_jp_add; returns -1 when nothing is stored. */
s32 reward_add_job_jp(s32 unit_index, s32 jp, s32 team, s32 slot) {
    unit_stats_t* unit;
    s32 total;
    s32 level;
    s32 levels;
    u8 flags;
    u8 level_byte;

    if (jp < 0) {
        return -1;
    }
    unit = &g_unit_stats[unit_index];
    if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
        return -1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
        & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD))) {
        return -1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE)) {
        return -1;
    }
    flags = unit->unit_flags;
    if (flags & UNIT_FLAG_MONSTER) {
        return -1;
    }
    if ((unit->initial_team_flags & UNIT_TEAM_MASK) != (team & 0xff)) {
        return -1;
    }
    if (slot == UNIT_CAREER_JOB_INDEX_BARD && (flags & UNIT_FLAG_FEMALE)) {
        return -1;
    }
    if (slot == UNIT_CAREER_JOB_INDEX_DANCER && (flags & UNIT_FLAG_MALE)) {
        return -1;
    }
    total = unit->career.job_points.current.values[slot] + jp;
    if (total > UNIT_JOB_POINT_MAX) {
        total = UNIT_JOB_POINT_MAX;
    }
    unit->career.job_points.current.values[slot] = total;
    total = unit->career.job_points.total.values[slot] + jp;
    if (total > UNIT_JOB_POINT_MAX) {
        total = UNIT_JOB_POINT_MAX;
    }
    unit->career.job_points.total.values[slot] = total;
    /* The job level comes from total JP; even slots take the high nibble. */
    level = job_level_calculate(total);
    levels = unit->career.job_levels[slot / 2];
    level_byte = level;
    if (slot & 1) {
        levels = level + (levels & 0xf0);
    } else {
        levels = (levels & 0xf) | (level_byte << 4);
    }
    unit->career.job_levels[slot / 2] = levels;
    job_unlocks_store(unit->career.unlocked_jobs, job_unlocks_calculate(unit->career.job_levels, unit->unit_flags));
    return 0;
}

/* Find a free tile near a unit for it to be moved to, returning 0 or -1.
 *
 * Panels holding another unit or an earlier destination are marked in
 * unit_record_index. Rings of increasing Manhattan radius around the unit are then
 * searched, first up to radius 3 within -10..+4 of the unit's height, then up
 * to radius 16 at any height, for a flat enough, unblocked tile that is not
 * lava or an obstacle. The chosen tile is also appended to the destination
 * list.
 *
 * The panel clear walks the byte offset itself: the target loop has no index
 * register, which an index loop keeps. Only level's destination store is an
 * assignment chain; that shape alone gives the target's register choices. */
s32 unit_find_relocation_tile(s32 unit_index, battle_map_tile_coordinates_t* destination) {
    unit_stats_t* unit;
    battle_map_tile_t* tile;
    s32 origin_x;
    s32 origin_y;
    s32 low;
    s32 high;
    s32 i;
    s32 n;
    s32 height;
    s32 pass;
    s32 limit;
    s32 dx;
    s32 dy;
    s32 x;
    s32 y;
    s32 side;
    s32 level;
    s32 index;
    s32 surface;
    u32 depth;
    battle_map_tile_flags_t flags;
    s32 found_x;
    s32 found_y;
    s32 total;
    target_panel_t* panel;

    unit = &g_unit_stats[unit_index];
    origin_x = unit->x;
    origin_y = unit->position.y;
    for (n = 0x1ff * sizeof(target_panel_t); n >= 0; n -= sizeof(target_panel_t)) {
        ((target_panel_t*)((u8*)g_target_panels + n))->unit_record_index = 0;
    }
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        unit = &g_unit_stats[i];
        if (unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE) {
            index = battle_map_tile_calculate_unit_index(unit);
            panel = &g_target_panels[index];
            tile = &g_battle_map_tile_data[index];
            if (i == unit_index) {
                height = tile->height * 2 + tile->depth_half_height.half_height;
                low = height - 10;
                if (low < 0) {
                    low = 0;
                }
                high = height + 4;
            } else {
                panel->unit_record_index = 1;
            }
        }
    }
    for (i = 0; i < g_unit_relocated_count; i++) {
        index = g_unit_relocated_coords[i].layer * 256 + g_unit_relocated_coords[i].y * g_battle_map_terrain_width
            + g_unit_relocated_coords[i].x;
        panel = &g_target_panels[index];
        panel->unit_record_index = 1;
    }
    /* Radius i rings: dx runs across the ring and dy = i - |dx| gives the two
     * tiles above and below it. */
    for (pass = 0; pass < 2; pass++) {
        limit = 4;
        if (pass != 0) {
            limit = 17;
        }
        for (i = 1; i < limit; i++) {
            for (dx = -i; dx <= i; dx++) {
                x = origin_x + dx;
                if (x < 0 || x >= g_battle_map_terrain_width) {
                    continue;
                }
                if (dx > 0) {
                    dy = i - dx;
                } else {
                    dy = i + dx;
                }
                side = -1;
                found_x = x;
                for (y = origin_y - dy; side < 2; side += 2) {
                    if (y >= 0 && y < g_battle_map_terrain_depth) {
                        found_y = y;
                        for (level = 0; level < 2; level++) {
                            index = level * 256 + g_battle_map_terrain_width * y + x;
                            tile = &g_battle_map_tile_data[index];
                            /* Byte access retains the original packed load and field-extraction order. */
                            depth = (*(u8*)&tile->depth_half_height);
                            if (tile->depth_half_height.depth >= 3) {
                                continue;
                            }
                            height = depth & BATTLE_MAP_TILE_HALF_HEIGHT_MASK;
                            if (height >= 3) {
                                continue;
                            }
                            flags = tile->flags;
                            if (flags.blocked) {
                                continue;
                            }
                            if (flags.untargetable) {
                                continue;
                            }
                            if (g_target_panels[index].unit_record_index != 0) {
                                continue;
                            }
                            surface = tile->surface.type;
                            if (surface == BATTLE_MAP_SURFACE_LAVA || surface == BATTLE_MAP_SURFACE_OBSTACLE) {
                                continue;
                            }
                            if (pass == 0) {
                                total = tile->height * 2 + height;
                                if (total > high || total < low) {
                                    continue;
                                }
                            }
                            destination->x = found_x;
                            g_unit_relocated_coords[g_unit_relocated_count].x = found_x;
                            g_unit_relocated_coords[g_unit_relocated_count].y = destination->y = found_y;
                            g_unit_relocated_coords[g_unit_relocated_count].layer = destination->layer = level;
                            g_unit_relocated_count++;
                            return 0;
                        }
                    }
                    y += dy * 2;
                }
            }
        }
    }
    return -1;
}

/* Combine a unit's movement abilities and statuses into its effective movement mode: Float status
 * counts as Float, Chicken and Frog cancel Fly. Fly or Teleport is kept alongside Float, Float
 * drops the water modes, and Silent Walk always carries over. */
move_effective_flags_e move_get_effective_flags(const unit_stats_t* unit) {
    u8 flags;
    s32 movement_abilities_2;
    s32 movement_abilities_3;
    s32 current_status_3 = unit->status_sets.current[2];

    movement_abilities_3 = unit->ability_flags.movement[2];
    movement_abilities_2 = unit->ability_flags.movement[1];

    flags = 0;
    if (current_status_3 & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
        movement_abilities_3 |= UNIT_MOVEMENT_SET_3_FLOAT;
    }
    if (current_status_3 & (STATUS_BYTE_MASK(STATUS_ID_CHICKEN) | STATUS_BYTE_MASK(STATUS_ID_FROG))) {
        movement_abilities_3 &= (u8)~UNIT_MOVEMENT_SET_3_FLY;
    }
    if (movement_abilities_3 & UNIT_MOVEMENT_SET_3_MOVE_IN_WATER) {
        flags = MOVE_EFFECTIVE_ON_WATER;
    } else if (movement_abilities_3 & UNIT_MOVEMENT_SET_3_WALK_ON_WATER) {
        flags = MOVE_EFFECTIVE_IN_WATER;
    }
    if (movement_abilities_3 & UNIT_MOVEMENT_SET_3_SILENT_WALK) {
        flags |= MOVE_EFFECTIVE_SILENT_WALK;
    }
    if (movement_abilities_3 & UNIT_MOVEMENT_SET_3_FLY) {
        flags |= MOVE_EFFECTIVE_FLY;
        if (movement_abilities_3 & UNIT_MOVEMENT_SET_3_FLOAT) {
            /* The target also preserves undefined effective-movement bit 0x04. */
            flags = (flags & (MOVE_EFFECTIVE_FLY | MOVE_EFFECTIVE_TELEPORT | 0x04 | MOVE_EFFECTIVE_SILENT_WALK))
                | MOVE_EFFECTIVE_FLOAT;
        }
    } else if (movement_abilities_2 & (UNIT_MOVEMENT_SET_2_TELEPORT | UNIT_MOVEMENT_SET_2_TELEPORT_2)) {
        flags |= MOVE_EFFECTIVE_TELEPORT;
        if (movement_abilities_3 & UNIT_MOVEMENT_SET_3_FLOAT) {
            flags = (flags & (MOVE_EFFECTIVE_FLY | MOVE_EFFECTIVE_TELEPORT | 0x04 | MOVE_EFFECTIVE_SILENT_WALK))
                | MOVE_EFFECTIVE_FLOAT;
        }
    } else if (movement_abilities_3 & UNIT_MOVEMENT_SET_3_FLOAT) {
        /* The target preserves Silent Walk and undefined bit 0x04 here. */
        flags = (flags & 0x06) | MOVE_EFFECTIVE_FLOAT;
    } else if (movement_abilities_3 & UNIT_MOVEMENT_SET_3_MOVE_IN_WATER) {
        flags |= MOVE_EFFECTIVE_ON_WATER;
    } else if (movement_abilities_3 & UNIT_MOVEMENT_SET_3_WALK_ON_WATER) {
        flags |= MOVE_EFFECTIVE_IN_WATER;
    }
    return flags;
}

/* Amount Move-Get EXP or Move-Get JP gives: the number of steps walked
 * (g_move_path.count), rnd(1..Move) for a teleport and 1 for a failed one,
 * at most 99. */
u8 reward_calculate_move_get_amount(const unit_stats_t* unit) {
    u8 amount = g_move_path.count;

    if (amount == MOVE_PATH_TELEPORT) {
        amount = (rand() * unit->move) / 0x8000 + 1;
    }
    if (amount == MOVE_PATH_TELEPORT_FAILED) {
        amount = 1;
    }
    if (amount >= 100) {
        amount = 99;
    }
    return amount;
}

/* Run the ability formula for a preview of attacker's command against target,
 * leaving the projected result in the target's action data.
 *
 * The attacker's action command is saved, replaced by command, and restored
 * afterwards. Without the Auto flag in the loaded ability data, the target's
 * tile must be among the action's targeted panels; otherwise its action data is
 * cleared to a miss with 0 accuracy. Always returns 0. */
s32 action_calculate_projected_effect(unit_stats_t* attacker, unit_stats_t* target, action_command_t* command) {
    u8 flags_3;
    s32 result;
    s32 saved_action_state;
    s32 saved_action_context;

    action_command_copy(&attacker->action_command, &g_action_saved_command);
    action_command_copy(command, &attacker->action_command);
    strike_data_init(attacker);
    saved_action_state = g_action_state;
    g_action_state = ACTION_STATE_PREVIEW;
    if (!(g_action_loaded_ability_flags_1 & ABILITY_SECONDARY_FLAG_1_AUTO)) {
        result = action_tiles_calculate(command, &flags_3);
        if (result != -1
            && !g_battle_map_tile_data[battle_map_tile_calculate_unit_index(target)].ceiling_depth_and_marks.targeted) {
            result = -1;
        }
        if (result == -1) {
            action_clear_current_data(&target->action);
            target->action.hit = 0;
            target->action.attack_accuracy = 0;
            g_action_state = saved_action_state;
            action_command_copy(&g_action_saved_command, &attacker->action_command);
            return 0;
        }
    }
    saved_action_context = g_action_context;
    g_action_work.defaulted_to_attack = 0;
    g_action_context = ACTION_CONTEXT_PRIMARY;
    target->action.reaction_id = 0;
    action_calculate_target_result(&attacker->action_command, target->battle_id);
    g_action_context = saved_action_context;
    g_action_state = saved_action_state;
    action_command_copy(&g_action_saved_command, &attacker->action_command);
    return 0;
}

/* The Elemental ability that the surface type of the unit's tile selects. */
s32 ability_get_elemental_id(const unit_stats_t* unit) {
    return g_terrain_geomancy_abilities[g_battle_map_tile_data[battle_map_tile_calculate_unit_index(unit)]
            .surface.type];
}
