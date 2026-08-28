#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Rolls the reward of a successful proposition.
 *
 * Reward kind 1 or 2 (proposition field 10) picks a random script variable
 * still clear in 0x321..0x33f or 0x350..0x35f, falling back to any index;
 * otherwise field 11 == 1 rolls a three-tier random reward (60/30/10%).
 * The chosen pair from proposition data records 10/11 then adds its bonus
 * to every participant's row 0 and sets the reward value. The target places
 * the random-reward branch last, hence the goto. */
void wldcore_proposition_roll_bonus_reward(void) {
    s32 candidates[32];
    u16* bonus_table;
    u16* value_table;
    u8* location_pairs;
    u8* random_pairs;
    s32 count;
    s32 i;
    s32 index;
    s32 participant_count;
    u16 type;
    s32 bonus;

    bonus_table = (u16*)wldcore_proposition_get_data_pointer(8);
    value_table = (u16*)wldcore_proposition_get_data_pointer(9);
    location_pairs = (u8*)wldcore_proposition_get_data_pointer(10);
    random_pairs = (u8*)wldcore_proposition_get_data_pointer(11);
    g_wldcore_job_selection.reward_type = 0;
    g_wldcore_job_selection.reward_value = 0;
    if (g_wldcore_job_selection.result != 0) {
        return;
    }
    type = g_wldcore_selected_proposition_row[0].fields.discovery_kind;
    if (type == 1) {
        g_wldcore_job_selection.reward_type = 1;
        count = 0;
        for (i = 0; i < 31; i++) {
            if (world_script_get_variable(i + 0x321) == 0) {
                candidates[count] = i;
                count++;
            }
        }
    } else if (type == 2) {
        count = 0;
        g_wldcore_job_selection.reward_type = type;
        for (i = 0; i < 16; i++) {
            if (world_script_get_variable(i + 0x350) == 0) {
                candidates[count] = i;
                count++;
            }
        }
    } else {
        goto random_reward;
    }
    if (count != 0) {
        index = (rand() * count) >> 15;
        g_wldcore_job_selection.reward_index = candidates[index];
    } else if (g_wldcore_job_selection.reward_type == 1) {
        g_wldcore_job_selection.reward_index = (rand() * 31) >> 15;
    } else {
        g_wldcore_job_selection.reward_index = (rand() * 16) >> 15;
    }
    participant_count = g_main_active_propositions[g_wldcore_job_selection.proposition_index].participant_count;
    index = (g_wldcore_selected_proposition_row[0].fields.category_index - 1) * 2;
    for (i = 0; i < participant_count; i++) {
        bonus = bonus_table[location_pairs[index + 1] - 1];
        g_wldcore_job_selection.rows[0][i] += bonus;
    }
    g_wldcore_job_selection.reward_value = value_table[location_pairs[index] - 1];
    return;

random_reward:
    if (g_wldcore_selected_proposition_row[0].fields.random_bonus != 1) {
        return;
    }
    g_wldcore_job_selection.reward_type = 3;
    index = (rand() * 100) >> 15;
    if (index < 40) {
        if (index < 10) {
            index = 2;
        } else {
            index = 1;
        }
    } else {
        index = 0;
    }
    index += (g_wldcore_selected_proposition_row[0].fields.category_index - 1) * 3;
    g_wldcore_job_selection.reward_index = index;
    participant_count = g_main_active_propositions[g_wldcore_job_selection.proposition_index].participant_count;
    for (i = 0; i < participant_count; i++) {
        bonus = bonus_table[random_pairs[index * 2 + 1] - 1];
        g_wldcore_job_selection.rows[0][i] += bonus;
    }
    g_wldcore_job_selection.reward_value = value_table[random_pairs[index * 2] - 1];
}
