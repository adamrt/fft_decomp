#include "fft/battle_ai.h"
#include "fft/data.h"
#include "fft/unit_slots.h"

/*
 * Initialize AI workspace defaults and present-unit indexing.
 *
 * Average maximum HP includes all present slots and is capped at 255.
 */
void battle_ai_init_workspace(void) {
    s32 i = 0;
    s32 present_count = 0;
    s32 total_max_hp = 0;
    battle_ai_data_t* ai = &g_battle_ai_data_base;

    g_battle_ai_workspace = ai;
    g_battle_ai_unit_snapshots = g_battle_ai_unit_snapshot_storage;
    ai->decision_state = 0;
    ai->selected_action.rank_byte = 0;
    ai->selected_action.coords.bytes.zero = 0;
    ai->hardcoded_status_abilities[0].skillset_flags.bytes.skillset = SKILLSET_ID_ELMDOR_BLOOD_SUCK;
    ai->hardcoded_status_abilities[0].id.packed_id = ABILITY_ID_ELMDOR_BLOOD_SUCK;
    ai->hardcoded_status_abilities[0].skillset_flags.bytes.usage_flags
        = BATTLE_AI_ABILITY_ENTRY_USABLE | BATTLE_AI_ABILITY_ENTRY_EXHAUSTIVE_ORIGIN_SCAN;
    ai->hardcoded_status_abilities[1].skillset_flags.bytes.skillset = SKILLSET_ID_FROG_ATTACK;
    ai->hardcoded_status_abilities[1].id.packed_id = ABILITY_ID_FROG_ATTACK;
    ai->hardcoded_status_abilities[1].skillset_flags.bytes.usage_flags
        = BATTLE_AI_ABILITY_ENTRY_USABLE | BATTLE_AI_ABILITY_ENTRY_EXHAUSTIVE_ORIGIN_SCAN;
    ai->present_unit_average_max_hp = 0;
    ai->map_max_x = g_map_max_x;
    ai->map_max_y = g_map_max_y;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        ai->unit_battle_ids[i] = 0xff;
        if (g_battle_unit_stats[i].entd_slot != BATTLE_ENTD_SLOT_NONE) {
            ai->unit_battle_ids[i] = present_count;
            battle_ai_init_unit_abilities(i);
            present_count++;
            total_max_hp += g_battle_unit_stats[i].max_hp;
        }
    }
    if (present_count != 0) {
        s32 average = total_max_hp / present_count;
        if (average >= 256) {
            ai->present_unit_average_max_hp = 255;
        } else {
            ai->present_unit_average_max_hp = average;
        }
    }
    {
        s32 empty = 0xff;
        for (i = 3; i >= 0; i--) {
            ai->coords_181c[i].bytes.x = empty;
        }
    }
    ai->throw_ability_id = ABILITY_ID_THROW_KNIFE;
    ai->throw_weapon_id = 1;
}
