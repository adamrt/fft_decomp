#include "fft/battle.h"
#include "fft/data.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "fft/main_unit.h"
#include "fft/map.h"
#include "psx/types.h"

/* Builds and previews a reaction using the current attacker's action and weapons.
 *
 * Returns 0 for an ineligible unit, -1 if targeting or preview fails, and 1
 * on success. Failed targeting or preview clears both borrowed weapon slots.
 * The menu type stays byte-sized, and the facing subtraction and action-field
 * assignments retain the target's evaluation order. The tile index and saved
 * action state share a temporary because their lifetimes do not overlap.
 */
s32 battle_action_set_mimic_ability(battle_stats_t* unit) {
    battle_stats_t* self;
    battle_ai_command_action_t action;
    battle_ai_command_action_t* current;
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

    self = unit;
    current = &g_current_ability_attacker.action;
    if (battle_action_can_unit_react(self) != 0) {
        return 0;
    }
    if ((self->team_flags & 0x30) != g_current_ability_attacker.team) {
        return 0;
    }
    if (self->misc_unit_id == current->unit_id) {
        return 0;
    }
    menu_type = g_main_action_menu_types_by_skillset[current->skillset];
    /* The target loads the ability id signed (lh). */
    ability_id = (s16)g_current_ability_attacker.action.ability_id;
    if (menu_type == 0 || menu_type == ACTION_MENU_TYPE_ARITHMETICKS || menu_type == ACTION_MENU_TYPE_MONSTER) {
        if (g_main_ability_range_data[ability_id].flags_3 & 0x10) {
            return 0;
        }
    }
    facing = self->position.bits.facing;
    attacker_facing = g_current_ability_attacker.facing - 4;
    direction = facing - attacker_facing;
    if (menu_type == ACTION_MENU_TYPE_JUMP) {
        direction |= 0x100;
    }
    if (battle_map_resolve_rotated_offset_layer(self, g_current_ability_attacker.target_delta_x,
            g_current_ability_attacker.target_delta_y, direction, &x, &y, &z)
        != 0) {
        return -1;
    }
    main_util_copy_action_data((const u8*)current, (u8*)&action);
    action.targeting_type = 5;
    action.unit_id = self->misc_unit_id;
    unit_id = self->misc_unit_id;
    action.target_x = x;
    action.target_elevation = z;
    weapon_id = g_current_ability_attacker.right_hand_weapon_id;
    action.target_y = y;
    action.target_id = unit_id;
    self->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = weapon_id;
    self->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = g_current_ability_attacker.left_hand_weapon_id;
    result = battle_target_set_panels_for_action((u8*)&action);
    if (result == -1 || result == 3
        || (result < 2
            && !(g_battle_map_tile_data[saved_state = (z << 8) + (g_map_max_x * y) + x].ceiling_depth_and_marks
                & MAP_TILE_FLAG_ABILITY_RANGE))) {
        self->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = ITEM_ID_NONE;
        self->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = ITEM_ID_NONE;
        return -1;
    }
    saved_state = g_battle_action_state;
    g_battle_action_state = BATTLE_ACTION_STATE_PREVIEW;
    result = battle_action_prepare_attack(&action, (battle_ai_command_action_t*)&self->action_actor_id, 1);
    self->charged_ability_ct = 0xff;
    main_status_change_unit(self, 0, 0xd, 1);
    g_battle_action_state = saved_state;
    if (result < 0) {
        self->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = ITEM_ID_NONE;
        self->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = ITEM_ID_NONE;
        return -1;
    }
    return 1;
}
