#include "fft/battle.h"
#include "psx/types.h"

/* Current action block of Miscellaneous unit_t Data (0x18c..0x1ad), reached
 * through one pointer: the target addresses these fields relative to
 * misc + 0x18c. */
typedef struct battle_action_misc_data {
    u8 action_18c;           /* 0x00 (misc 0x18c) */
    u8 target_count;         /* 0x01 (misc 0x18d) */
    u8 target_list[0x10];    /* 0x02 (misc 0x18e) */
    u8 animate_on_miss_flag; /* 0x12 (misc 0x19e) */
    u8 control_value_19f;    /* 0x13 */
    u16 last_attack_id;      /* 0x14 */
    u8 ability_formula;      /* 0x16 */
    u8 reaction_occurred;    /* 0x17 (misc 0x1a3) */
    u8 continue_attack;      /* 0x18 */
    u8 current_hit_number;   /* 0x19 */
    u8 reaction_id_1a6;      /* 0x1a */
    u8 _unused_1b;           /* 0x1b */
    u8 target_new_x;         /* 0x1c */
    u8 target_new_y;         /* 0x1d */
    u8 target_new_map_level; /* 0x1e */
    u8 used_weapon_id;       /* 0x1f */
    s16 reaction_ability_id; /* 0x20 (misc 0x1ac) */
} battle_action_misc_data_t;

/* Blaze Gun, Glacier Gun and Blast Gun (item ids 0x4a..0x4c) play the weapon
 * strike even when an ability is used. */
#define IS_ELEMENTAL_GUN(id) (((id) == 0x4a || (id) == 0x4b) || (id) == 0x4c)

/* Finalizes the casting unit's action and picks its attack animation.
 *
 * Each target's attack result is resolved first; for Fall (ability 0x200) in
 * the later phase the targets are moved to the knockback destination instead.
 * An ability picks the ability animation unless a reaction occurred, while a
 * plain attack or an elemental gun picks the weapon strike.
 *
 * continue_attack_count is nonzero on the follow-up strikes of a continued
 * attack. */
void battle_action_set_target_coords_and_attacker_anim(void) {
    battle_unit_misc_data_t* unit;
    battle_unit_misc_data_t* target;
    battle_action_misc_data_t* action;
    s32 i;

    g_animation_speed = 1;
    g_battle_game_state = BATTLE_GAME_STATE_SECONDARY_EFFECT;
    unit = battle_unit_get_casting_misc_data();
    unit->state_frame_counter = 0;
    if (unit->continue_attack_count == 0) {
        battle_unit_face_towards_action_target(unit, 0);
    }
    action = (battle_action_misc_data_t*)&unit->action_18c;
    if (action->target_count != 0) {
        for (i = 0; i < action->target_count; i++) {
            target = battle_unit_get_misc_data_by_battle_id(action->target_list[i]);
            if (target != 0) {
                target->pending_attack_result
                    = battle_action_finalize_attack_and_flag_reactions(target->battle_data->misc_unit_id);
                if (target->pending_attack_result == -1) {
                    battle_unit_find_relocation_tile(target->battle_data->misc_unit_id, &target->dismount);
                }
            }
        }
    }

    if (g_battle_action_phase == 1) {
        if (unit->used_ability_id != 0 && !IS_ELEMENTAL_GUN(unit->used_item_or_weapon_id)
            && action->reaction_occurred == 0) {
            battle_unit_select_attack_animation_for_ability(
                unit, battle_unit_get_misc_data_by_battle_id(action->target_list[0]));
        } else if (unit->used_ability_id == 0 || IS_ELEMENTAL_GUN(unit->used_item_or_weapon_id)) {
            if (action->target_count != 0) {
                battle_unit_select_weapon_attack_animation(
                    unit, battle_unit_get_misc_data_by_battle_id(action->target_list[0]));
            } else {
                battle_unit_select_weapon_attack_animation(unit, 0);
            }
        }
    } else if (unit->used_ability_id == 0x200) {
        for (i = 0; i < unit->target_count; i++) {
            target = battle_unit_get_misc_data_by_battle_id(action->target_list[i]);
            if (target != 0) {
                target->map_x = unit->target_new_x;
                target->map_y = unit->target_new_y;
                target->map_z = unit->target_new_map_level;
                battle_unit_set_tile_position(target->battle_data->misc_unit_id, target->map_x, target->map_y,
                    target->map_z, (s16)target->facing / 1024);
                battle_unit_set_move_and_screen_coords(target);
            }
        }
    } else if (action->reaction_ability_id != ABILITY_ID_REACTION_REFLECT) {
        if (unit->used_ability_id != 0 && !IS_ELEMENTAL_GUN(unit->used_item_or_weapon_id)
            && unit->continue_attack_count == 0) {
            if (action->reaction_occurred == 0) {
                battle_unit_select_attack_animation_for_ability(
                    unit, battle_unit_get_misc_data_by_battle_id(action->target_list[0]));
            }
        } else if (unit->used_ability_id == 0 || IS_ELEMENTAL_GUN(unit->used_item_or_weapon_id)) {
            if (action->target_count != 0) {
                battle_unit_select_weapon_attack_animation(
                    unit, battle_unit_get_misc_data_by_battle_id(action->target_list[0]));
            } else {
                battle_unit_select_weapon_attack_animation(unit, 0);
            }
        }
    }
}
