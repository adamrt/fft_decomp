#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/*
 * The .ld places .rodata at 0x80174068 so the switch's jump table lands on the
 * target's table.
 */

/* Build the targeting panels for an action and report how its target is chosen.
 *
 * The skillset's menu type picks the range. Abilities (the default and monster
 * menus, the Elemental ability of the unit's tile, the Draw Out of the chosen
 * katana) use their range data through target_set_ability_panels. Item
 * reaches 1 tile, or 4 with Throw Item or for a Mime. Throw reaches the unit's
 * Move and excludes its own tile. Attack and Charge use the weapon's range, Jump
 * the unit's Jump abilities.
 *
 * Returns a target_panels_e value: INVALID for an absent unit, a katana or
 * ability id out of range or a menu type without an action; NO_TARGETING for the
 * remaining menu types and for an auto ability with range 0 that does not use the
 * weapon's range; NONE when no tile is marked; TILE_ONLY when the action cannot
 * follow a unit (always for Item, Throw, Attack, Charge and Jump); otherwise
 * UNIT_OR_TILE. */
s32 target_set_panels_for_action(const action_command_t* source) {
    action_command_t action;
    unit_stats_t* unit;
    ability_secondary_data_t* ability;
    s32 mode;
    s32 range;
    s32 result;
    u8 flags;
    u8 item;
    u16 ability_id;

    main_util_copy_action_data(source, &action);
    mode = 0;
    if (action.actor_id >= BATTLE_UNIT_SLOT_COUNT) {
        return TARGET_PANELS_INVALID;
    }
    unit = &g_battle_unit_stats[action.actor_id];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return TARGET_PANELS_INVALID;
    }
    switch (g_main_action_menu_types_by_skillset[action.skillset_id]) {
    case ACTION_MENU_TYPE_DEFAULT:
    case ACTION_MENU_TYPE_MONSTER:
        ability_id = action.ability_id;
        mode = 1;
        break;
    case ACTION_MENU_TYPE_ITEM_INVENTORY:
        range = ((unit->ability_flags.support[2] & BATTLE_SUPPORT_SET_3_THROW_ITEM) || unit->job_id == JOB_ID_MIME) ? 4
                                                                                                                    : 1;
        mode = 2;
        break;
    case ACTION_MENU_TYPE_WEAPON_INVENTORY:
        mode = 2;
        range = unit->move | 0x80;
        break;
    case ACTION_MENU_TYPE_ELEMENTS:
        ability_id = g_main_terrain_geomancy_abilities[g_map_tile_data[map_calculate_location(unit)].surface.type];
        mode = 1;
        break;
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        mode = 1;
        item = action.item_or_equipment;
        if (item < ITEM_ID_ASURA_KNIFE || item > ITEM_ID_CHIRIJIRADEN) {
            return TARGET_PANELS_INVALID;
        }
        ability_id = (u8)action.item_or_equipment + (ABILITY_ID_DRAW_OUT_ASURA - ITEM_ID_ASURA_KNIFE);
        break;
    case ACTION_MENU_TYPE_ATTACK:
    case ACTION_MENU_TYPE_CHARGE:
        result = target_set_weapon_attack_panels(&action);
        flags = ABILITY_SECONDARY_FLAG_2_NO_FOLLOW_TARGET;
        break;
    case ACTION_MENU_TYPE_JUMP:
        result = target_set_jump_ability_panels(&action);
        flags = ABILITY_SECONDARY_FLAG_2_NO_FOLLOW_TARGET;
        break;
    case ACTION_MENU_TYPE_BLANK_05:
    case ACTION_MENU_TYPE_UNKNOWN_0F:
        return TARGET_PANELS_INVALID;
    default:
        return TARGET_PANELS_NO_TARGETING;
    }
    if (mode == 1) {
        if (ability_id >= ABILITY_ID_ITEM_FIRST) {
            return TARGET_PANELS_INVALID;
        }
        ability = &g_main_ability_range_data[ability_id];
        range = ability->range;
        flags = ability->flags_2;
        if ((ability->flags_1 & ABILITY_SECONDARY_FLAG_1_AUTO) && range == 0
            && !(ability->flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE)) {
            return TARGET_PANELS_NO_TARGETING;
        }
        action.ability_id = ability_id;
        result = target_set_ability_panels(&action);
    }
    if (mode == 2) {
        result = target_calculate_ability_range_with_map_parameters(unit, range & 0xff);
        flags = ABILITY_SECONDARY_FLAG_2_NO_FOLLOW_TARGET;
    }
    if (result != 0) {
        /* The named temporary is load-bearing; the direct test compiles differently. */
        s32 flag_result = flags & ABILITY_SECONDARY_FLAG_2_NO_FOLLOW_TARGET;

        return flag_result != 0;
    }
    return TARGET_PANELS_NONE;
}
