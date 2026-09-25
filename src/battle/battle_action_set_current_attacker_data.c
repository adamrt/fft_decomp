#include "fft/battle.h"
#include "psx/types.h"

void battle_action_set_current_attacker_data(battle_stats_t* unit) {
    battle_stats_t* target;
    s32 x;
    s32 y;

    if (unit->job_id == JOB_ID_MIME) {
        return;
    }
    main_util_copy_action_data(&unit->action_actor_id, (u8*)&g_current_ability_attacker.action);
    if (unit->action_target_kind == BATTLE_ACTION_TARGET_UNIT) {
        target = &g_battle_unit_stats[unit->action_target_id];
        x = target->x;
        y = target->position.bits.y;
    } else {
        /* The target reads only the low byte (lbu) of the s16 target
         * coordinates here. */
        x = *(u8*)&unit->action_target_x;
        y = *(u8*)&unit->action_target_y;
    }
    g_current_ability_attacker.target_delta_x = x - unit->x;
    g_current_ability_attacker.target_delta_y = y - unit->position.bits.y;
    /* Read through a cast pointer, not position.raw: GCC 2.6.3 assumes a
     * struct-member load cannot alias the scalar global stores above and
     * hoists it past them; the target keeps source order. */
    g_current_ability_attacker.facing = (*(u16*)&unit->position >> 8) & 0xF;
    g_current_ability_attacker.team = unit->team_flags & BATTLE_TEAM_MASK;
    g_current_ability_attacker.right_hand_weapon_id = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
    g_current_ability_attacker.left_hand_weapon_id = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
    g_current_ability_attacker.data_initialized = 1;
}
