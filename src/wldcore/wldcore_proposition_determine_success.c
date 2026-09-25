#include "fft/wldcore.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Scores the current proposition's success chance.
 *
 * A dispatched proposition (flags bit 2) clears the outer score rows and
 * reports result 2. Otherwise each participant's score (row 1) accumulates
 * four proposition data tables (records 3..6) keyed on the proposition's
 * fields, the participant's job category and its bravery, faith and level
 * tiers; the total maps onto result 0 (>= 0x63), 1 (>= 0x3b) or 2. A named
 * preferred job then gives a 60% roll to promote the result back to 0. The
 * preferred-job scan reads the party record once, with the scoring loop's
 * leftover index, as the target does. The target builds the row walk pointers
 * from the proposition_index address, which needs the rows and that word to be
 * members of one object (g_wldcore_job_selection). */
void wldcore_proposition_determine_success(void) {
    const u8* job_table_a;
    const u8* job_table_b;
    const u8* tier_table_a;
    const u8* tier_table_b;
    party_data_t* unit;
    s32 participant_count;
    s32 total;
    s32 score;
    s32 tier;
    s32 value;
    s32 i;

    participant_count = g_main_active_propositions[g_wldcore_job_selection.proposition_index].participant_count;
    job_table_a = (const u8*)wldcore_proposition_get_data_pointer(3);
    job_table_b = (const u8*)wldcore_proposition_get_data_pointer(4);
    tier_table_a = (const u8*)wldcore_proposition_get_data_pointer(5);
    total = 0;
    tier_table_b = (const u8*)wldcore_proposition_get_data_pointer(6);

    if (g_main_active_propositions[g_wldcore_job_selection.proposition_index].flags & 2) {
        for (i = 0; i < participant_count; i++) {
            g_wldcore_job_selection.rows[2][i] = 0;
            g_wldcore_job_selection.rows[0][i] = 0;
        }
        g_wldcore_job_selection.gate = 1;
        g_wldcore_job_selection.result = 2;
        g_wldcore_job_selection.reward_type = 0;
        return;
    }

    g_wldcore_job_selection.gate = 0;
    for (i = 0; i < participant_count; i++) {
        unit = wldcore_get_party_data_pointer(
            g_main_active_propositions[g_wldcore_job_selection.proposition_index].participant_indices[i]);

        g_wldcore_job_selection.rows[1][i]
            = job_table_a[((s32)g_wldcore_selected_proposition_row[0].fields.category_index - 1) * 22
                + wldcore_map_job_id_to_category_index(unit->job_id)];
        g_wldcore_job_selection.rows[1][i]
            += job_table_b[((s32)g_wldcore_selected_proposition_row[0].fields.job_list_index - 1) * 22
                + wldcore_map_job_id_to_category_index(unit->job_id)];

        value = unit->bravery;
        if (value != 0) {
            tier = (value - 1) / 20;
        } else {
            tier = 0;
        }
        g_wldcore_job_selection.rows[1][i]
            += tier_table_a[((s32)g_wldcore_selected_proposition_row[0].fields.job_list_index - 1) * 5 + tier];

        value = unit->faith;
        if (value != 0) {
            tier = (value - 1) / 20;
        } else {
            tier = 0;
        }
        g_wldcore_job_selection.rows[1][i]
            += tier_table_b[((s32)g_wldcore_selected_proposition_row[0].fields.job_list_index - 1) * 5 + tier];

        value = unit->level;
        if (value != 0) {
            tier = (value - 1) / 10;
        } else {
            tier = 0;
        }
        score = g_wldcore_job_selection.rows[1][i]
            + tier_table_a[((s32)g_wldcore_selected_proposition_row[0].fields.job_list_index - 1) * 10 + tier];
        total += score;
        g_wldcore_job_selection.rows[1][i] = score;
    }

    g_wldcore_job_selection.result = 0;
    if (total < 0x63) {
        g_wldcore_job_selection.result = 1;
    }
    if (total < 0x3B) {
        g_wldcore_job_selection.result = 2;
    }
    if (g_wldcore_job_selection.result == 0) {
        return;
    }
    if (g_wldcore_selected_proposition_row[0].fields.preferred_job == 0) {
        return;
    }

    unit = wldcore_get_party_data_pointer(
        g_main_active_propositions[g_wldcore_job_selection.proposition_index].participant_indices[i]);
    for (i = 0; i < participant_count; i++) {
        if ((s32)g_wldcore_selected_proposition_row[0].fields.preferred_job - 1
            == wldcore_map_job_id_to_category_index(unit->job_id)) {
            if (((rand() * 100) >> 15) < 0x3D) {
                g_wldcore_job_selection.result = 0;
                return;
            }
        }
    }
}
