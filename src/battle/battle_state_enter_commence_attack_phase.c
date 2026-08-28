#include "fft/battle.h"
#include "psx/libc.h"
#include "psx/types.h"

/*
 * Copies the caster's action target data (battle_stats_t 0x16e..0x181) into
 * its Misc unit_t Data at 0x15c..0x16f, then latches the used ability id.
 */
void battle_state_enter_commence_attack_phase(void) {
    battle_stats_t* stats;
    battle_stats_t** stats_pointer;
    battle_unit_misc_data_t* unit;

    battle_state_stop_game_flow();
    g_battle_game_state = BATTLE_GAME_STATE_COMMENCE_ATTACK_PHASE;
    unit = battle_unit_get_source_misc_data();
    stats_pointer = &unit->battle_data;
    stats = unit->battle_data;
    memcpy((u8*)unit + 0x15c, &stats->action_actor_id, 0x10);
    memcpy((u8*)unit + 0x16c, &stats->action_target_y, 4);
    /* last_ability_id is s16 in the header; the target loads it with lhu. */
    unit->used_ability_id = *(u16*)&(*stats_pointer)->last_ability_id;
    unit->ability_ct_resolved |= 2;
    if (unit->used_ability_id != 0) {
        battle_unit_start_ability_charge_animation_for_movement(unit);
    }
}
