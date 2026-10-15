#include "fft/battle_action.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_status.h"
#include "fft/battle_unit.h"
#include "fft/memory.h"
#include "fft/unit.h"
#include "psx/types.h"

/* Disable the renderer flags of every current status, then clear the unit's
 * innate, immune, current, status CT and inflicted status bytes (Morbol
 * transformation preparation). */
void status_reapply_active_flags(unit_stats_t* unit) {
    s32 i;
    s32 battle_id = unit->battle_id;

    for (i = 0; i < STATUS_COUNT; i++) {
        s32 byte_index = i / 8;
        s32 mask = 0x80 >> (i & 7);
        if (unit->status_sets.current[byte_index] & mask) {
            status_queue_runtime_graphics_flag_change(STATUS_HANDLER_INDEX(i), 0, battle_id);
        }
    }
    mem_zero(unit->status_sets.innate, 0x1F);
    mem_zero(unit->inflicted_status, STATUS_BYTE_COUNT);
}

/* Enable the renderer status flags for each of a unit's current statuses.
 * Returns -1 for an empty unit slot, otherwise 0. */
s32 status_init_special_flag_enabling(s32 unit_id) {
    unit_stats_t* unit;
    s32 i;

    unit = &g_battle_unit_stats[unit_id];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    for (i = 0; i < STATUS_COUNT; i++) {
        s32 byte_index = i / 8;
        s32 mask = 0x80 >> (i & 7);
        if (unit->status_sets.current[byte_index] & mask) {
            status_queue_runtime_graphics_flag_change(STATUS_HANDLER_INDEX(i), 1, unit_id);
        }
    }
    return 0;
}

/* Index of the unit's tile in g_map_tile_data: layer * 256 + y * map width
 * + x, with the layer in bit 15 of the packed position. */
s32 map_calculate_location(const unit_stats_t* unit) {
    u8 y = unit->position.y;
    u16 elevation = unit->position.higher_elevation;
    u8 x = unit->x;
    return (elevation << 8) + (y * g_map_max_x) + x;
}

/* The battle stats of an occupied unit slot, or 0 for an empty slot or an id past
 * the last slot. */
unit_stats_t* battle_unit_get_existing_pointer(u32 battle_id) {
    unit_stats_t* unit = 0;
    if (battle_id < BATTLE_UNIT_SLOT_COUNT) {
        unit = &g_battle_unit_stats[battle_id];
        if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
            unit = 0;
        }
    }
    return unit;
}

/* The first unit whose has_turn is set (the unit taking its turn), or 0. */
unit_stats_t* battle_unit_find_active_data_pointer(void) {
    s32 i = 0;
    unit_stats_t* unit = g_battle_unit_stats;
    for (; i < BATTLE_UNIT_SLOT_COUNT; i++, unit++) {
        if (unit->has_turn != 0) {
            return unit;
        }
    }
    return 0;
}

/* The idle action menu for a unit id: battle_menu_get_unit_action_menu_id, with
 * the four TURN menus replaced by their RIDER_TURN variants for a rider. Returns
 * BATTLE_MENU_ID_NONE for an empty slot. */
battle_menu_id_e battle_menu_get_id_based_on_mount_moveable_actable(s32 battle_id) {
    unit_stats_t* unit;
    s32 menu;

    unit = battle_unit_get_existing_pointer(battle_id);
    if (unit == 0) {
        return BATTLE_MENU_ID_NONE;
    }
    menu = battle_menu_get_unit_action_menu_id(unit);
    if ((unit->mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER) != 0) {
        switch (menu) {
        case BATTLE_MENU_ID_TURN:
            menu = BATTLE_MENU_ID_RIDER_TURN;
            break;
        case BATTLE_MENU_ID_TURN_MOVED:
            menu = BATTLE_MENU_ID_RIDER_TURN_MOVED;
            break;
        case BATTLE_MENU_ID_TURN_ACTED:
            menu = BATTLE_MENU_ID_RIDER_TURN_ACTED;
            break;
        case BATTLE_MENU_ID_TURN_MOVED_ACTED:
            menu = BATTLE_MENU_ID_RIDER_TURN_MOVED_ACTED;
            break;
        default:
            return menu;
        }
    }
    return menu;
}

/* Pick a unit's idle action menu (battle_menu_id_e), before the rider variants.
 *
 * While the unit taking its turn is not a manually controlled player unit, every
 * unit gets an inspect menu: PLAYER_UNIT_INSPECT or OTHER_UNIT_INSPECT. Otherwise
 * the unit with the turn gets one of the four TURN menus, by whether its move is
 * gone (moved, or Don't Move) and its action is gone (acted, or any action
 * restriction), and any other unit gets PLAYER_UNIT or OTHER_UNIT.
 *
 * The local can_evade holds the action restriction. */
battle_menu_id_e battle_menu_get_unit_action_menu_id(const unit_stats_t* unit) {
    unit_stats_t* act;
    s32 flag;
    s32 player_controlled;
    s32 player_flag;
    s32 dont_move;
    s32 can_evade;
    u8 moved;
    u8 acted;

    flag = 0;
    act = battle_unit_find_active_data_pointer();
    if (act != 0) {
        if ((act->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) == 0
            || act->auto_battle_setting != AI_AUTOBATTLE_MANUAL) {
            flag = 1;
        }
    }
    player_flag = unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED;
    player_controlled = player_flag != 0;
    if (flag != 0) {
        if (player_controlled != 0) {
            return BATTLE_MENU_ID_PLAYER_UNIT_INSPECT;
        }
        return BATTLE_MENU_ID_OTHER_UNIT_INSPECT;
    }
    dont_move = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_MOVE)];
    can_evade = battle_unit_get_action_restriction(unit);
    moved = unit->movement_taken | (dont_move & STATUS_BYTE_MASK(STATUS_ID_DONT_MOVE));
    acted = unit->action_taken | can_evade;
    if (unit->has_turn != 0) {
        if (moved == 0 && acted == 0) {
            return BATTLE_MENU_ID_TURN;
        }
        if (moved != 0 && acted == 0) {
            return BATTLE_MENU_ID_TURN_MOVED;
        }
        if (moved == 0 && acted != 0) {
            return BATTLE_MENU_ID_TURN_ACTED;
        }
        return BATTLE_MENU_ID_TURN_MOVED_ACTED;
    }
    if (player_controlled != 0) {
        return BATTLE_MENU_ID_PLAYER_UNIT;
    }
    return BATTLE_MENU_ID_OTHER_UNIT;
}

/* Returns the unit's battle_unit_action_restriction_e.
 *
 * CANNOT_ACT for Don't Act or a ridden mount. DEEP_WATER on a tile of depth 2 or
 * more, unless the unit has Float, Chicken or Frog, one of the Move in Water,
 * Walk on Water or Float movement abilities, or is a rider. */
battle_unit_action_restriction_e battle_unit_get_action_restriction(const unit_stats_t* unit) {
    s32 depth;

    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_ACT)] & STATUS_BYTE_MASK(STATUS_ID_DONT_ACT))
        != 0) {
        return BATTLE_UNIT_ACTION_RESTRICTION_CANNOT_ACT;
    }
    if ((unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) != 0) {
        return BATTLE_UNIT_ACTION_RESTRICTION_CANNOT_ACT;
    }
    depth = g_map_tile_data[map_calculate_location(unit)].depth_half_height.depth;
    if (depth < 2) {
        return BATTLE_UNIT_ACTION_RESTRICTION_NONE;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
            & (STATUS_BYTE_MASK(STATUS_ID_FLOAT) | STATUS_BYTE_MASK(STATUS_ID_CHICKEN)
                | STATUS_BYTE_MASK(STATUS_ID_FROG)))
        != 0) {
        return BATTLE_UNIT_ACTION_RESTRICTION_NONE;
    }
    if ((unit->ability_flags.movement[2]
            & (BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER | BATTLE_MOVEMENT_SET_3_WALK_ON_WATER | BATTLE_MOVEMENT_SET_3_FLOAT))
        != 0) {
        return BATTLE_UNIT_ACTION_RESTRICTION_NONE;
    }
    return ((unit->mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER) == 0) << 1;
}

/* Builds a unit's 0xff-terminated action-menu skillset list and loads each
 * entry's abilities.
 *
 * `menu_types` receives each entry's low-nibble menu type and `empty` is set
 * to 1 where battle_menu_load_skillset returned 0. A monster skillset in either
 * slot yields a single unloaded primary entry and returns 1; a unit that cannot
 * act or is carrying a rider gets an empty list. Returns -1 when the unit does
 * not exist, otherwise 0.
 *
 * The walk rewinds `entries` from `list` rather than reading `list` in the
 * loop: that keeps `entries` as CSE's canonical copy, so the list stores use
 * the incremented pointer instead of offsets from the saved base. `supports`
 * is declared first so it wins the allocation tie with `primary`. */
s32 battle_menu_build_unit_action_menus(s32 unit_id, u8* entries, u8* empty, u8* menu_types) {
    unit_stats_t* unit;
    u8* list;
    u8 supports;
    u8 primary;
    u8 secondary;
    s32 i;
    u8 id;
    u8 menu_type;

    unit = battle_unit_get_existing_pointer(unit_id);
    if (unit == 0) {
        return -1;
    }
    primary = unit->primary_skillset;
    secondary = unit->secondary_skillset;
    list = entries;
    if (primary >= SKILLSET_ID_MONSTER_FIRST || secondary >= SKILLSET_ID_MONSTER_FIRST) {
        entries[0] = primary;
        empty[0] = 0;
        menu_types[0] = g_main_action_menu_types_by_skillset[primary];
        entries[1] = SKILLSET_ID_NONE;
        return 1;
    }
    supports = unit->ability_flags.support[3];
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG)) {
        supports = 0;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_ACT)] & STATUS_BYTE_MASK(STATUS_ID_DONT_ACT))
        || (unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT)) {
        *entries = SKILLSET_ID_NONE;
        return 0;
    }
    *entries++ = SKILLSET_ID_ATTACK;
    if (primary != 0 && primary != SKILLSET_ID_MIMIC) {
        *entries++ = primary;
    }
    if (secondary != 0) {
        *entries++ = secondary;
    }
    if (supports & BATTLE_SUPPORT_SET_4_DEFEND) {
        *entries++ = SKILLSET_ID_DEFEND;
    }
    if (supports & BATTLE_SUPPORT_SET_4_EQUIP_CHANGE) {
        *entries++ = SKILLSET_ID_EQUIP_CHANGE;
    }
    *entries = SKILLSET_ID_NONE;
    entries = list;
    for (i = 0; i < 6; i++) {
        id = entries[i];
        if (id == SKILLSET_ID_NONE) {
            break;
        }
        menu_type = g_main_action_menu_types_by_skillset[id] & 0xf;
        menu_types[i] = menu_type;
        if (battle_menu_load_skillset(unit_id, id, menu_type) == 0) {
            empty[i] = 1;
        } else {
            empty[i] = 0;
        }
    }
    return 0;
}

/* Loads the unit's usable abilities for an action menu type into scratch lists.
 *
 * A frog only keeps the default, blank, monster and attack menus. Returns the
 * menu loader's result, or whether the menu type is below 15 for menus without
 * a loader. */
s32 battle_menu_load_skillset(s32 unit_id, u8 skillset, u8 menu) {
    u8 items[0x58];
    s16 abilities[0x44];
    u8 mp_costs[0x18];
    u8 ability_ct[0x18];
    u8 ability_flags[0x18];
    u8 turns[0x18];
    s32 result;

    if ((g_battle_unit_stats[unit_id].status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)]
            & STATUS_BYTE_MASK(STATUS_ID_FROG))
        && menu != ACTION_MENU_TYPE_DEFAULT && menu != ACTION_MENU_TYPE_MONSTER && menu != ACTION_MENU_TYPE_ATTACK
        && menu != ACTION_MENU_TYPE_BLANK_05) {
        return 0;
    }
    switch (menu) {
    case ACTION_MENU_TYPE_DEFAULT:
        return battle_menu_get_unit_skillset_ability_data(
            unit_id, skillset, abilities, mp_costs, ability_ct, 0, ability_flags, turns);
    case ACTION_MENU_TYPE_ITEM_INVENTORY:
        return battle_menu_load_item_abilities(unit_id, skillset, items);
    case ACTION_MENU_TYPE_WEAPON_INVENTORY:
        return battle_menu_collect_throwable_items(unit_id, skillset, items);
    case ACTION_MENU_TYPE_ARITHMETICKS:
        result = battle_menu_load_math_skill_attributes(unit_id, skillset, abilities);
        if (result != 0) {
            result = battle_menu_load_math_skill_multiples(unit_id, skillset, abilities);
            if (result != 0) {
                result = battle_menu_collect_calculator_abilities(unit_id, skillset, abilities);
            }
        }
        return result;
    case ACTION_MENU_TYPE_ELEMENTS:
        return battle_menu_load_elemental_abilities(unit_id, skillset);
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        return battle_menu_collect_draw_out_katanas(unit_id, skillset, items);
    case ACTION_MENU_TYPE_CHARGE:
        return battle_menu_call_load_charge_skillset(unit_id, skillset, abilities);
    default:
        return menu < 15;
    }
}

/* Count the AT-list entries that resolve before an action by battle_id
 * charging for ct; returns 0xff when no such position exists within 40. */
u32 action_get_number_of_turns_to_resolve(s32 battle_id, s32 ct, const battle_at_entry_t* entries) {
    s32 count;
    if (g_battle_between_turn_resume_state == BATTLE_BETWEEN_TURN_STATE_PICK_UNIT_AFTER_CHARGE && ct != 0) {
        ct += 1;
    }
    ct <<= 8;
    for (count = 0; count < BATTLE_AT_LIST_CAPACITY; count++, entries++) {
        s32 entry_unit = entries->unit_code;
        s32 is_action = entry_unit & BATTLE_AT_ACTION_ENTRY;
        s32 entry_ct;
        entry_unit &= BATTLE_AT_UNIT_ID_MASK;
        if (entry_unit == BATTLE_AT_END_OF_LIST)
            return 0xFF;
        entry_ct = entries->sort_key;
        if (entry_ct >= ct) {
            if (ct != entry_ct)
                return count & 0xFF;
            if (battle_id == entry_unit && is_action != 0)
                return count & 0xFF;
            if (entry_unit >= battle_id)
                return count & 0xFF;
            if (is_action == 0)
                return count & 0xFF;
        }
    }
    return 0xFF;
}

/* Build the parallel ability lists for one of a unit's skillsets.
 *
 * Every learned slot of the skillset is listed in ability_ids (terminated with
 * -1); the count is returned. Ordinary abilities also report MP cost (halved
 * by Half of MP), CT (halved by Short Charge, zeroed by Non-Charge unless the
 * ability perseveres), reflectable/silence flags and the number of turns until
 * it resolves; Charge abilities (0x196-0x19d) report CT and turns only. A Frog
 * may list only Frog (0x1d), and abilities requiring a sword or Materia Blade
 * are skipped when the unit lacks one.
 *
 * The two flag bytes need separate locals: sharing one keeps the attribute
 * byte in a globally allocated register, which the target does not. */
s32 battle_menu_get_unit_skillset_ability_data(
    s32 unit_id, u8 skillset, s16* ability_ids, u8* mp_out, u8* ct_out, s32 unused, u8* flags_out, u8* turns_out) {
    battle_at_entry_t at_list[BATTLE_AT_LIST_CAPACITY];
    unit_stats_t* unit;
    ability_secondary_data_t* secondary;
    s32 known;
    s16 ability;
    s32 materia_blade;
    s32 sword;
    s32 charge;
    s32 half_mp;
    s32 frog;
    s32 count;
    s32 i;
    s32 skillset_index;
    s32 mp;
    s32 ct;
    s32 kind;
    u8 requirements;
    u8 attributes;
    s32 turns;

    count = 0;
    materia_blade = 0;
    sword = 0;
    charge = 0;
    half_mp = 0;
    frog = 0;
    unit = battle_unit_get_existing_pointer(unit_id);
    if (unit == 0) {
        return 0;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG)) { /* Frog */
        frog = 1;
    }
    if (unit->ability_flags.support[1] & BATTLE_SUPPORT_SET_2_HALF_MP) {
        half_mp = 1;
    }
    if (unit->ability_flags.support[3] & BATTLE_SUPPORT_SET_4_NON_CHARGE) {
        charge = 2;
    } else if (unit->ability_flags.support[3] & BATTLE_SUPPORT_SET_4_SHORT_CHARGE) {
        charge = 1;
    }
    action_calculate_at_list(at_list, BATTLE_AT_LIST_MODE_ALL);
    if (unit->equipped_flags & BATTLE_UNIT_EQUIPPED_FLAG_MATERIA_BLADE) {
        materia_blade = 1;
    }
    if (unit->equipped_flags & BATTLE_UNIT_EQUIPPED_FLAG_SWORD) {
        sword = 1;
    }
    if (unit->unit_flags & UNIT_FLAG_MONSTER) {
        skillset_index = -1;
    } else if (skillset >= SKILLSET_ID_BASIC_SKILL && skillset < SKILLSET_ID_MIMIC) {
        skillset_index = skillset - SKILLSET_ID_BASIC_SKILL;
    } else if (skillset == unit->primary_skillset || skillset == unit->base_job_skillset) {
        skillset_index = 0;
    } else {
        skillset_index = -1;
    }
    if (skillset_index == -1) {
        known = 0xffffff;
    } else {
        known = main_ability_get_known(unit, skillset_index);
    }
    for (i = 0; i < 0x10; i++) {
        if (!(known & (0x800000 >> i))) {
            continue;
        }
        ability = main_ability_get_id_from_skillset(skillset, i);
        if (frog && ability != ABILITY_ID_BLACK_MAGIC_FROG) {
            continue;
        }
        if (ability == 0) {
            continue;
        }
        if (ability >= 1 && ability < ABILITY_ID_ITEM_FIRST) {
            secondary = &g_main_ability_range_data[ability];
            requirements = secondary->flags_4;
            if ((requirements & ABILITY_SECONDARY_FLAG_4_REQUIRES_SWORD) && !sword) {
                continue;
            }
            if ((requirements & ABILITY_SECONDARY_FLAG_4_REQUIRES_MATERIA_BLADE) && !materia_blade) {
                continue;
            }
            mp = secondary->mp_cost;
            if (half_mp) {
                mp = (mp + 1) >> 1;
            }
            ct = secondary->ct & 0x7f;
            attributes = secondary->flags_3;
            kind = (attributes >> 7) | ((attributes & ABILITY_SECONDARY_FLAG_3_AFFECTED_BY_SILENCE) >> 4);
            if (!(attributes & ABILITY_SECONDARY_FLAG_3_PERSEVERE)) {
                if (charge == 1) {
                    ct = (ct + 1) >> 1;
                }
                if (charge == 2) {
                    ct = 0;
                }
            }
            turns = action_get_number_of_turns_to_resolve(unit_id, ct, at_list);
            mp_out[count] = mp;
            ct_out[count] = ct;
            flags_out[count] = kind;
            turns_out[count] = turns;
        } else if (ability >= ABILITY_ID_CHARGE_FIRST && ability < ABILITY_ID_MATH_FIRST) {
            ct = (g_main_charge_ability_data - ABILITY_ID_CHARGE_FIRST)[ability].ct;
            turns = action_get_number_of_turns_to_resolve(unit_id, ct, at_list);
            mp_out[count] = 0;
            ct_out[count] = ct;
            flags_out[count] = 0;
            turns_out[count] = turns;
        }
        *ability_ids++ = ability;
        count++;
    }
    *ability_ids = -1;
    return count;
}

/* Loads the unit's Item skillset entries through battle_menu_load_item_abilities and returns the
 * low byte of its result (the entry count). */
s32 battle_menu_display_item_inventory_ability(s32 unit_id, s32 skillset, u8* out_item_ids) {
    return battle_menu_load_item_abilities(unit_id, skillset & 0xFF, out_item_ids) & 0xFF;
}

/*
 * Collects the item abilities the unit can use into out_item_ids, terminated
 * by 0xff, and returns 0x100 plus the number of entries (0 when the unit or
 * the skillset is wrong, or the menu is empty).
 *
 * Loading the ability id into `item_index` before subtracting ABILITY_ID_ITEM_FIRST
 * keeps the table index in its own s16 pseudo, so GCC emits the target's fresh
 * `addiu v1,-0x170` on the sign-extended 16-bit value instead of reusing the
 * masked `id + 0xfe90` sum the range test already holds (the two differ only
 * above bit 15); spelled as one expression it is one instruction short.
 */
s32 battle_menu_load_item_abilities(s32 unit_id, u8 skillset, u8* out_item_ids) {
    unit_stats_t* unit;
    u16 abilities[0x14];
    u8 mp_costs[0x18];
    u8 ability_ct[0x18];
    u8 ability_flags[0x18];
    u8 turns[0x18];
    s32 count;
    s32 i;
    s32 found;
    u8* table;
    s16 item_index;

    found = 0;
    unit = battle_unit_get_existing_pointer(unit_id);
    if (unit == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_ITEM) {
        *out_item_ids = ITEM_ID_NONE;
        return 0;
    }
    count = battle_menu_get_unit_skillset_ability_data(
        unit_id, SKILLSET_ID_ITEM, (s16*)abilities, mp_costs, ability_ct, 0, ability_flags, turns);
    if (count == 0) {
        *out_item_ids = ITEM_ID_NONE;
        return 0;
    }
    for (i = 0; i < count; i++) {
        if (abilities[i] >= ABILITY_ID_ITEM_FIRST && abilities[i] < ABILITY_ID_THROW_FIRST) {
            table = g_main_item_ability_item_ids;
            item_index = abilities[i];
            item_index = item_index - ABILITY_ID_ITEM_FIRST;
            found++;
            *out_item_ids = table[item_index];
            out_item_ids++;
        }
    }
    *out_item_ids = ITEM_ID_NONE;
    return found + 0x100;
}

/* The number of items battle_menu_collect_throwable_items lists in out_item_ids:
 * its result without the 0x100 marker. */
s32 battle_menu_load_throw_abilities(s32 unit_id, s32 skillset, u8* out_item_ids) {
    return battle_menu_collect_throwable_items(unit_id, skillset & 0xFF, out_item_ids) & 0xFF;
}

/* Collect the item ids the unit may throw into `out`, terminated with -1.
 *
 * The skillset's abilities select item types through g_main_throw_ability_item_types; every item of
 * a selected type that is throwable and that the party still owns is listed.
 * */
s32 battle_menu_collect_throwable_items(s32 unit_id, u8 skillset, u8* out) {
    u16 abilities[0x14];
    u8 item_types[0x14];
    u8 mp_costs[0x14];
    u8 charge_times[0x14];
    u8 ability_flags[0x14];
    u8 turn_counts[0x14];
    struct unit_stats* unit;
    s32 count;
    s32 ability_count;
    s32 i;
    s32 j;
    s32 found;
    item_data_t* item;
    u8* type_table;
    s16 throw_index;
    u8 type;
    u8 item_id;

    count = 0;
    unit = battle_unit_get_existing_pointer(unit_id);
    if (unit == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_THROW
        || (ability_count = battle_menu_get_unit_skillset_ability_data(
                unit_id, SKILLSET_ID_THROW, (s16*)abilities, mp_costs, charge_times, 0, ability_flags, turn_counts))
            == 0) {
        *out = ITEM_ID_NONE;
        return 0;
    }
    for (i = 0; i < ability_count; i++) {
        if (abilities[i] >= ABILITY_ID_THROW_FIRST && abilities[i] < ABILITY_ID_JUMP_FIRST) {
            type_table = g_main_throw_ability_item_types;
            throw_index = abilities[i];
            throw_index = throw_index - ABILITY_ID_THROW_FIRST;
            item_types[i] = type_table[throw_index];
        }
    }
    for (i = 0; i < ITEM_ID_COUNT; i++) {
        item = &g_main_item_primary_data[i];
        type = item->type;
        found = 0;
        for (j = 0; j < ability_count; j++) {
            if (type == item_types[j]) {
                found = 1;
                break;
            }
        }
        if (found != 0 && (item->type_flags & ITEM_TYPE_FLAG_WEAPON) != 0
            && (g_main_item_weapon_data[item->secondary_data_id].flags & WEAPON_FLAG_THROWABLE) != 0) {
            item_id = i;
            if (action_decrement_player_item_quantity(unit, item_id, 0) == 0) {
                *out = item_id;
                out++;
                count++;
            }
        }
    }
    *out = ITEM_ID_NONE;
    return count + 0x100;
}

/* Collects the unit's learned Math Skill attribute abilities (CT, Level, Exp,
 * Height) into out_ability_ids. */
s32 battle_menu_load_math_skill_attributes(s32 unit_id, s32 skillset, s16* out_ability_ids) {
    return battle_menu_collect_math_skill_abilities_by_flags(
        unit_id, skillset & 0xFF, out_ability_ids, CALCULATOR_ATTRIBUTE_MASK);
}

/* Collects the unit's learned Math Skill divisor abilities (Prime, 5, 4, 3)
 * into out_ability_ids. */
s32 battle_menu_load_math_skill_multiples(s32 unit_id, s32 skillset, s16* out_ability_ids) {
    return battle_menu_collect_math_skill_abilities_by_flags(
        unit_id, skillset & 0xFF, out_ability_ids, CALCULATOR_MULTIPLE_MASK);
}

/* Collects the unit's learned Math Skill abilities (0x19e..0x1a5) whose
 * secondary-data flags intersect mask into out, -1-terminated, and returns
 * the count. */
s32 battle_menu_collect_math_skill_abilities_by_flags(s32 unit_id, u8 skillset, s16* out, u8 mask) {
    u16 abilities[0x14];
    u8 mp_costs[0x18];
    u8 charge_times[0x18];
    u8 ability_flags[0x18];
    u8 turn_counts[0x18];
    s32 count;
    s32 ability_count;
    s32 i;
    s16 ability;
    u8* secondary_flags;
    u8 flag_bits;

    count = 0;
    if (battle_unit_get_existing_pointer(unit_id) == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_MATH_SKILL) {
        *out = -1;
        return 0;
    }
    ability_count = battle_menu_get_unit_skillset_ability_data(
        unit_id, SKILLSET_ID_MATH_SKILL, (s16*)abilities, mp_costs, charge_times, 0, ability_flags, turn_counts);
    if (ability_count == 0) {
        return 0;
    }
    for (i = 0; i < ability_count; i++) {
        ability = abilities[i];
        if (ability >= ABILITY_ID_MATH_FIRST && ability < ABILITY_ID_REACTION_FIRST) {
            secondary_flags = &g_main_math_rsm_ability_data[ability - ABILITY_ID_MATH_FIRST];
            flag_bits = *secondary_flags;
            if (flag_bits & mask) {
                *out++ = ability;
                count++;
            }
        }
    }
    *out = -1;
    return count;
}

/*
 * Collects the unit's Calculator-eligible abilities from the four skillsets
 * at g_battle_menu_calculator_skillsets into out, -1-terminated, and returns the count. Only the Math
 * Skill skillset collects anything.
 *
 * The record pointer must be taken inside the inner loop: hoisting the table
 * base into a local before the loop swaps the two preheader address setups.
 */
s32 battle_menu_collect_calculator_abilities(s32 unit_id, u8 skillset, s16* out) {
    s16 abilities[0x14];
    u8 mp_costs[0x18];
    u8 charge_times[0x18];
    u8 ability_flags[0x18];
    u8 turn_counts[0x18];
    u8 skillsets[4];
    s32 count;
    s32 ability_count;
    s32 i;
    s32 j;
    s16 ability;
    ability_secondary_data_t* secondary;

    count = 0;
    memcpy(skillsets, g_battle_menu_calculator_skillsets, sizeof(skillsets));
    if (battle_unit_get_existing_pointer(unit_id) == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_MATH_SKILL) {
        *out = -1;
        return 0;
    }
    for (i = 0; i < 4; i++) {
        ability_count = battle_menu_get_unit_skillset_ability_data(
            unit_id, skillsets[i], abilities, mp_costs, charge_times, 1, ability_flags, turn_counts);
        if (ability_count != 0) {
            for (j = 0; j < ability_count; j++) {
                ability = abilities[j];
                secondary = &g_main_ability_range_data[ability];
                if (secondary->flags_3 & ABILITY_SECONDARY_FLAG_3_CALCULATOR_ELIGIBLE) {
                    *out++ = ability;
                    count++;
                }
            }
        }
    }
    *out = -1;
    return count;
}

/* Returns the Elemental (Geomancy) ability for the surface under the unit when
 * the unit knows and can use it, otherwise 0. */
u16 battle_menu_load_elemental_abilities(s32 unit_id, u8 skillset) {
    unit_stats_t* unit;
    s16 abilities[0x14];
    u8 mp_costs[0x18];
    u8 ability_ct[0x18];
    u8 ability_flags[0x18];
    u8 turns[0x18];
    s32 count;
    s32 i;
    s16 terrain_ability;

    unit = battle_unit_get_existing_pointer(unit_id);
    if (unit == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_ELEMENTAL) {
        return 0;
    }
    count = battle_menu_get_unit_skillset_ability_data(
        unit_id, SKILLSET_ID_ELEMENTAL, abilities, mp_costs, ability_ct, 1, ability_flags, turns);
    if (count == 0) {
        return 0;
    }
    terrain_ability = g_main_terrain_geomancy_abilities[g_map_tile_data[map_calculate_location(unit)].surface.type];
    for (i = 0; i < count; i++) {
        if (abilities[i] == terrain_ability) {
            return terrain_ability;
        }
    }
    return 0;
}

/*
 * Collect the monster-skill ability ids a unit can use into `out`, with one
 * range/charge flag byte per accepted id in `flags_out`.
 *
 * The target keeps two live copies of the ability id: `id` in $a0 feeds both
 * sign extensions and the `addiu -1` range check, while `ability` in $a1
 * carries the value the `sh` store writes and is shared with the berserk arm's
 * Frog Attack.
 */
s32 battle_menu_collect_monster_skill_abilities(s32 unit_id, u8 skillset, s16* out, s32 unused, u8* flags_out) {
    unit_stats_t* unit;
    s32 count;
    s32 berserk;
    s32 i;
    s16 id;
    s16 ability;
    u8 flags;

    count = 3;
    berserk = 0;
    unit = battle_unit_get_existing_pointer(unit_id);
    if (unit == 0) {
        return 0;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG)) {
        berserk = 1;
    }
    if (g_main_action_menu_types_by_skillset[skillset] != ACTION_MENU_TYPE_MONSTER) {
        *out = -1;
        return 0;
    }
    if (berserk != 0) {
        ability = ABILITY_ID_FROG_ATTACK;
        *out = ability;
        out++;
    }
    if (battle_menu_init_monster_skill_check(unit) != 0) {
        count = MONSTER_SKILLSET_ABILITY_COUNT;
    }
    for (i = 0; i < count; i++) {
        id = main_ability_get_id_from_skillset(skillset, i);
        ability = id;
        if ((berserk == 0 || id == ABILITY_ID_BLACK_MAGIC_FROG) && id != 0 && id >= 1 && id < ABILITY_ID_ITEM_FIRST) {
            flags = g_main_ability_range_data[id].flags_3;
            *flags_out++ = (flags >> 7) | ((flags & ABILITY_SECONDARY_FLAG_3_AFFECTED_BY_SILENCE) >> 4);
            *out = ability;
            out++;
        }
    }
}

/* Monster Skill check: the number of the four tiles adjacent to the unit that
 * hold a same-team Monster Skill supporter at the unit's total height. Returns 0
 * for an empty slot. */
s32 battle_menu_init_monster_skill_check(const unit_stats_t* unit) {
    battle_unit_height_data_t height_data;
    s32 team;
    s32 x;
    s32 y;
    s32 height;
    s32 sum;

    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return 0;
    }
    battle_unit_calculate_height_data(&height_data, unit->battle_id);
    team = unit->team_flags;
    x = unit->x;
    y = unit->position.y;
    height = height_data.total_height;
    sum = battle_menu_has_monster_skill_supporter_at_tile(team, x - 1, y, height)
        + battle_menu_has_monster_skill_supporter_at_tile(team, x + 1, y, height);
    sum += battle_menu_has_monster_skill_supporter_at_tile(team, x, y - 1, height);
    return sum + battle_menu_has_monster_skill_supporter_at_tile(team, x, y + 1, height);
}

/* Returns whether a same-team Monster Skill supporter occupies the supplied tile. */
s32 battle_menu_has_monster_skill_supporter_at_tile(s32 team_flags, s32 x, s32 y, s32 total_height) {
    battle_unit_height_data_t height;
    u8 reference_height;
    s32 unit_index;
    s32 unit_team_flags;

    unit_team_flags = team_flags;
    if (x < 0 || x >= g_map_max_x) {
        return 0;
    }
    if (y >= 0) {
        if (y >= g_map_max_y) {
            return 0;
        }

        unit_index = 0;
        reference_height = total_height;
        for (; unit_index < BATTLE_UNIT_SLOT_COUNT; unit_index++) {
            unit_stats_t* unit = &g_battle_unit_stats[unit_index];

            if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE || unit->x != x || unit->position.y != y
                || action_is_reaction_prevented(unit) != 0
                || !(unit->ability_flags.support[3] & BATTLE_SUPPORT_SET_4_MONSTER_SKILL)
                || ((unit->team_flags ^ unit_team_flags) & BATTLE_TEAM_MASK)) {
                continue;
            }
            battle_unit_calculate_height_data(&height, unit->battle_id);
            if ((reference_height - height.total_height) >= -5 && (reference_height - height.total_height) <= 5) {
                return 1;
            }
        }
    }

    return 0;
}

/* The number of katanas battle_menu_collect_draw_out_katanas lists in
 * out_item_ids: its result without the 0x100 marker. */
s32 battle_menu_load_draw_out_abilities(s32 unit_id, s32 skillset, u8* out_item_ids) {
    return battle_menu_collect_draw_out_katanas(unit_id, skillset & 0xFF, out_item_ids) & 0xFF;
}

/* Collects the Draw Out weapon entry (ability id - 0x26) of each of the unit's
 * usable Draw Out abilities into out_item_ids, 0xff-terminated, and returns
 * 0x100 plus their count (0 when the unit or skillset is wrong or the list is
 * empty). */
s32 battle_menu_collect_draw_out_katanas(s32 unit_id, u8 skillset, u8* out_item_ids) {
    u16 abilities[0x10];
    u8 mp_costs[0x10];
    u8 ability_flags[0x10];
    u8 ability_ct[0x10];
    u8 turns[0x10];
    s32 ability_count;
    s32 i;
    s32 count;
    u32 ability_id;
    u32 item_id;

    count = 0;
    if (battle_unit_get_existing_pointer(unit_id) == 0) {
        return 0;
    }
    if (skillset != SKILLSET_ID_DRAW_OUT) {
        *out_item_ids = ITEM_ID_NONE;
        return 0;
    }
    ability_count = battle_menu_get_unit_skillset_ability_data(
        unit_id, SKILLSET_ID_DRAW_OUT, (s16*)abilities, mp_costs, ability_ct, 0, ability_flags, turns);
    if (ability_count == 0) {
        *out_item_ids = ITEM_ID_NONE;
        return 0;
    }
    for (i = 0; i < ability_count; i++) {
        ability_id = abilities[i];
        if (ability_id < ABILITY_ID_ITEM_FIRST) {
            item_id = ability_id - (ABILITY_ID_DRAW_OUT_ASURA - ITEM_ID_ASURA_KNIFE);
            *out_item_ids = item_id;
            out_item_ids++;
            count++;
        }
    }
    *out_item_ids = ITEM_ID_NONE;
    return count + 0x100;
}

/* battle_menu_load_charge_skillset for a caller that only wants the ability ids;
 * the charge times and turn counts go to scratch arrays. */
s32 battle_menu_call_load_charge_skillset(s32 unit_id, u8 skillset_id, s16* ability_ids) {
    u8 charge_times[16];
    u8 turn_counts[16];
    return battle_menu_load_charge_skillset(unit_id, skillset_id, ability_ids, charge_times, turn_counts);
}

/* Loads the skillset's usable abilities with their CT and turn counts into the
 * caller's lists; the MP-cost and silence/reflect flag lists are scratch. */
s32 battle_menu_load_charge_skillset(s32 unit_id, s32 skillset, s16* abilities, u8* ability_ct, u8* turns) {
    u8 mp_costs[16];
    u8 ability_flags[16];
    return battle_menu_get_unit_skillset_ability_data(
        unit_id, skillset, abilities, mp_costs, ability_ct, 0, ability_flags, turns);
}
