#include "fft/battle.h"

/* Writes the acting unit's tile into unit's action fields (0x16e..) as the
 * target of skillset/ability and runs the attack preview.  Returns -1 when the
 * action itself fails, -2 when the acting unit's tile is not targetable and
 * -3 when the preview fails; otherwise the preview result. */
s32 battle_action_store_counter_ability(battle_stats_t* unit, s8 skillset_id, s16 ability_id, s32 validate_target) {
    u8* action;
    battle_stats_t* acting_unit;
    s32 result;
    s32 acting_unit_id;
    s32 preview_result;
    u8 saved_charged_ability_ct;

    acting_unit_id = g_battle_acting_unit_id;
    action = &unit->action_actor_id;
    unit->last_skillset_id = skillset_id;
    unit->last_ability_id = ability_id;
    acting_unit = &g_battle_unit_stats[acting_unit_id];
    if (validate_target != 0) {
        result = battle_target_set_panels_for_action(action);
        if (result == -1 || (u32)(result - 2) < 2) {
            return -1;
        }
        if ((g_battle_target_tile_targetable_flags[battle_map_calculate_location(acting_unit) * 8]
                & MAP_TILE_FLAG_ABILITY_RANGE)
            == 0) {
            return -2;
        }
    }
    /* The target stores through the action pointer (s0), not the unit:
     * 0x0a action_target_kind, 0x0b action_target_id, 0x0c action_target_x,
     * 0x0e action_target_elevation, 0x10 action_target_y. */
    action[0xa] = BATTLE_ACTION_TARGET_TILE;
    action[0xb] = g_battle_acting_unit_id_byte;
    *(s16*)(action + 0xc) = acting_unit->x;
    *(s16*)(action + 0x10) = acting_unit->position.bits.y;
    *(u16*)(action + 0xe) = acting_unit->position.raw >> 15;
    saved_charged_ability_ct = unit->charged_ability_ct;
    result = battle_action_call_attack_preparation_at_preview(action);
    unit->charged_ability_ct = saved_charged_ability_ct;
    if (result == -1 || (preview_result = (u32)(result - 2) < 2, preview_result != 0)) {
        preview_result = -3;
    }
    return preview_result;
}
