#include "fft/battle.h"

/*
 * Choose or resume an action imposed by an AI-control status.
 *
 * Chicken takes precedence over Blood Suck, Confusion and Berserk. Return
 * -1 to suspend, 1 when selection completes, or 0 if none of these applies.
 */
s32 battle_ai_choose_status_action(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    battle_stats_t* unit;
    battle_stats_t* berserk_unit;
    battle_ai_ability_entry_t* entry;
    s32 i, target_or_count;
    battle_ai_coords_t coords;
    s32 status_2;

    if (g_battle_ai_data_base.decision_state) {
        if (g_battle_ai_data_base.action_selection_phase == 0)
            goto chicken;
        goto movement;
    }
    unit = g_battle_ai_temp_unit_data;
    g_battle_ai_data_base.movement_scenario = 0;
    status_2 = unit->status_sets.current[2];
    if (status_2 & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHICKEN)) {
    chicken:
        if (battle_ai_score_reachable_tiles() == -1) {
            ai->action_selection_phase = 0;
            return -1;
        }
        battle_ai_clear_words((s32*)ai->tile_target_distance, sizeof(ai->tile_target_distance));
        battle_ai_select_destination(255, BATTLE_AI_DESTINATION_DISTANCE_FIRST);
        coords = ai->candidate_coords[0];
        ai->selected_action.target_flags_set = 0;
        goto save_movement;
    }
    if (unit->status_sets.current[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_BLOOD_SUCK)) {
        target_or_count = battle_ai_find_nearest_target(BATTLE_AI_NEAREST_WITHOUT_BLOOD_SUCK);
        entry = &ai->hardcoded_status_abilities[0];
        ai->hardcoded_status_abilities[0].id.packed_id |= g_battle_ai_data_base.acting_unit_id << 10;
    load_targeted_ability:
        battle_ai_load_ability_entry(entry);
        {
            /* Direct member indexing changes this reverse clear. Keep the
             * target's base cursor and derive the field offset from its member. */
            u32 cursor;
            i = BATTLE_UNIT_SLOT_COUNT - 1;
            cursor = (u32)ai + i;
            do {
                *(u8*)(cursor + ((u32)&ai->targetability.live.unit_targetable - (u32)ai)) = 0;
                i--;
                cursor--;
            } while (i >= 0);
        }
        ai->targetability.live.unit_targetable[target_or_count] = 1;
        if (battle_ai_choose_random_action())
            goto success;
    copy_target:
        battle_ai_transfer_unit_coordinates(target_or_count, &ai->considered_unit_coords);
    movement:
        if (battle_ai_fill_target_distance_grid(&ai->considered_unit_coords) == -1) {
            ai->action_selection_phase = 1;
            return -1;
        }
        battle_ai_select_destination(0x7fffffff, BATTLE_AI_DESTINATION_DISTANCE_FIRST);
        coords = ai->candidate_coords[ai->movement_scenario];
        ai->selected_action.target_flags_set = 0;
    save_movement:
        /* Movement-only selection leaves the other action fields unchanged. */
        ai->selected_action.skillset = 0;
        ai->selected_action.coords = coords;
        goto success;
    }
    if (unit->status_sets.current[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CONFUSION)) {
        if (battle_ai_select_active_units(BATTLE_AI_UNIT_FILTER_ANY, 255)) {
            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                ai->targetability.live.unit_targetable[i] = 0;
                if (!battle_ai_check_target_type(i))
                    ai->targetability.live.unit_targetable[i] = ai->targetability.live.unit_active[i];
            }
            /* The aligned skillset/usage halfword preserves target LHU/SH;
             * byte-only usage accesses change the instructions. */
            target_or_count = 0;
            for (i = 0; i < 34; i++) {
                entry = &ai->ability_lists[ai->acting_unit_battle_id][i];
                if (entry->skillset_flags.bytes.skillset == SKILLSET_ID_NONE)
                    break;
                if (entry->skillset_flags.packed & 0x8000)
                    target_or_count++;
            }
            if (target_or_count) {
                do {
                    i = rand() % target_or_count;
                    {
                        s32 n;
                        for (n = 0; n < 34; n++) {
                            entry = &ai->ability_lists[ai->acting_unit_battle_id][n];
                            if (entry->skillset_flags.packed & 0x8000) {
                                if (i == 0)
                                    break;
                                i--;
                            }
                        }
                    }
                    battle_ai_load_ability_entry(entry);
                    if (battle_ai_choose_random_action())
                        goto success;
                    target_or_count--;
                    entry->skillset_flags.packed &= 0x7fff;
                } while (target_or_count);
            }
        }
        target_or_count = battle_ai_find_nearest_target(BATTLE_AI_NEAREST_UNIT);
        goto copy_target;
    }
    if (status_2 & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_BERSERK)) {
        target_or_count = battle_ai_find_nearest_target(BATTLE_AI_NEAREST_ENEMY);
        berserk_unit = g_battle_ai_temp_unit_data;
        if (berserk_unit->primary_skillset < SKILLSET_ID_MONSTER_FIRST
            && !(berserk_unit->status_sets.current[2] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG))) {
            entry = &ai->ability_lists[g_battle_ai_data_base.acting_unit_battle_id][0];
        } else {
            entry = &ai->hardcoded_status_abilities[1];
            ai->hardcoded_status_abilities[1].id.packed_id |= ai->acting_unit_id << 10;
        }
        goto load_targeted_ability;
    }
    return 0;
success:
    return 1;
}
