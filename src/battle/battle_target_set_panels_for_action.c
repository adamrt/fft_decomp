/*
 * The .ld places .rodata at 0x80174068 so the switch's jump table lands on the
 * target's table.
 */
#include "fft/battle.h"
#include "fft/data.h"
#include "fft/job.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "fft/map.h"
#include "psx/types.h"

s32 battle_target_set_panels_for_action(u8* source) {
    battle_ai_command_action_t action;
    battle_stats_t* unit;
    ability_secondary_data_t* ability;
    s32 mode;
    s32 range;
    s32 result;
    u8 flags;
    u16 ability_id;

    main_util_copy_action_data(source, (u8*)&action);
    mode = 0;
    if (action.unit_id >= 21) {
        return -1;
    }
    unit = &g_battle_unit_stats[action.unit_id];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    switch (g_main_action_menu_types_by_skillset[action.skillset]) {
    case ACTION_MENU_TYPE_DEFAULT:
    case ACTION_MENU_TYPE_MONSTER:
        ability_id = action.ability_id;
        mode = 1;
        break;
    case ACTION_MENU_TYPE_ITEM_INVENTORY:
        range = ((unit->support_abilities[2] & 8) || unit->job_id == JOB_ID_MIME) ? 4 : 1;
        mode = 2;
        break;
    case ACTION_MENU_TYPE_WEAPON_INVENTORY:
        mode = 2;
        range = unit->move | 0x80;
        break;
    case ACTION_MENU_TYPE_ELEMENTS:
        ability_id
            = g_geomancy_terrain_ability_table[g_battle_map_tile_data[battle_map_calculate_location(unit)].surface.value
                & 0x3f];
        mode = 1;
        break;
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        mode = 1;
        if ((u32)(action.item_id - 0x26) >= 0xa) {
            return -1;
        }
        ability_id = action.item_id + 0x26;
        break;
    case ACTION_MENU_TYPE_ATTACK:
    case ACTION_MENU_TYPE_CHARGE:
        result = battle_target_set_weapon_attack_panels(&action);
        flags = 0x10;
        break;
    case ACTION_MENU_TYPE_JUMP:
        result = battle_target_set_jump_ability_panels((const u8*)&action);
        flags = 0x10;
        break;
    case ACTION_MENU_TYPE_BLANK_05:
    case ACTION_MENU_TYPE_UNKNOWN_0F:
        return -1;
    default:
        return 2;
    }
    if (mode == 1) {
        if (ability_id >= 0x170) {
            return -1;
        }
        ability = &g_main_ability_range_data[ability_id];
        range = ability->range;
        flags = ability->flags_2;
        if ((ability->flags_1 & 2) && range == 0 && !(ability->flags_1 & 0x20)) {
            return 2;
        }
        action.ability_id = ability_id;
        /* battle_target_set_ability_panels returns nothing; the target still
         * takes $v0 as the result. */
        result = ((s32 (*)(const u8*))battle_target_set_ability_panels)((const u8*)&action);
    }
    if (mode == 2) {
        result = battle_target_calculate_ability_range_with_map_parameters(unit, range & 0xff);
        flags = 0x10;
    }
    if (result != 0) {
        /* The named temporary is load-bearing; the direct test compiles differently. */
        s32 flag_result = flags & 0x10;

        return flag_result != 0;
    }
    return 3;
}
