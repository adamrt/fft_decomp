#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

void battle_action_set_damage_display_type_based_on_ability(void) {
    battle_unit_misc_data_t* misc;
    u16 raw;
    s32 ability;
    s32 target;

    g_animation_speed = 1;
    misc = battle_unit_get_casting_misc_data();
    battle_action_resolve_ability_strike(misc->battle_data->misc_unit_id, (battle_strike_work_t*)&misc->action_18c);
    raw = misc->last_attack_id;
    /* The target reads 0x1a6 as a signed halfword. */
    target = *(s16*)&misc->reaction_id_1a6;
    ability = raw & 0xffff;
    misc->used_ability_id = raw;
    battle_gfx_set_thrown_item_graphic_palette(misc->used_weapon_id, misc);
    if (((target != 0) && (ability != target)) || (ability == 0x200) || (ability == 0)
        || (battle_effect_load_ability(ability) != 0)) {
        battle_action_set_target_coords_and_attacker_anim();
        return;
    }
    g_battle_game_state = BATTLE_GAME_STATE_START_EFFECT_FILE_OPEN;
    battle_effect_store_targets_and_display_types(ability, misc);
}
