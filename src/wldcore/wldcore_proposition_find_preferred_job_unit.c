/* The target keeps &g_wldcore_job_selection.proposition_index in s5 and
 * reaches excluded_party_index, 12 bytes below, off the same base, which is
 * why it saves one more callee-saved register than a per-field access would.
 *
 * `base` stays an explicit local so the job-list offset is computed once in
 * the preheader and the inner add uses two registers. The slot test is
 * written `slots[0] > slots[1]`; the reversed spelling swaps the slots[] load
 * pair and costs instructions.
 *
 * The empty asm statement is explained where it stands. */
#include "fft/data.h"
#include "fft/wldcore.h"
#include "psx/types.h"

s32 wldcore_proposition_find_preferred_job_unit(void) {
    s32 candidates[4];
    s32 slots[4];
    s32 count;
    s32 i;
    s32 k;
    s32 participant_count;
    s32 party_index;
    s32 base;
    const wldcore_proposition_data_t* messages;
    party_data_t* unit;

    participant_count = g_main_active_propositions[g_wldcore_job_selection.proposition_index].participant_count;
    messages = wldcore_proposition_get_data_pointer(0xE);
    count = 0;

    if (g_wldcore_selected_proposition_row[0].fields.preferred_job != 0) {
        for (i = 0; i < participant_count; i++) {
            party_index = g_main_active_propositions[g_wldcore_job_selection.proposition_index].participant_indices[i];
            unit = wldcore_get_party_data_pointer(party_index);
            if ((s32)g_wldcore_selected_proposition_row[0].fields.preferred_job - 1
                == wldcore_map_job_id_to_category_index(unit->job_id)) {
                candidates[count] = party_index;
                count++;
            }
        }
    }

    if (count != 0) {
        for (i = 0; i < count; i++) {
            if (candidates[i] != g_wldcore_job_selection.excluded_party_index) {
                return candidates[i] + 1;
            }
        }
        goto first;
    }

    base = ((s32)g_wldcore_selected_proposition_row[0].fields.job_list_index - 1) * 9;
    for (i = 0; i < participant_count; i++) {
        party_index = g_main_active_propositions[g_wldcore_job_selection.proposition_index].participant_indices[i];
        if (party_index != g_wldcore_job_selection.excluded_party_index) {
            unit = wldcore_get_party_data_pointer(party_index);
            for (k = 0; k < 9; k++) {
                if (messages->message_adjustments[base + k] == unit->job_id) {
                    candidates[count] = party_index;
                    slots[count] = k;
                    count++;
                    break;
                }
            }
        }
    }

    if (count == 1) {
        goto first;
    }
    if (count != 2) {
        return 0;
    }
    if (slots[0] > slots[1]) {
        goto second;
    }
    /* Shared exits: with structured returns, cross-jumping keeps a different
     * copy of `candidates[0] + 1` and the arms swap places. */
first:
    return candidates[0] + 1;
second: {
    /* Keeps cross-jumping from merging the scan's early return into this arm instead of the first. */
    s32 result = candidates[1] + 1;
    __asm__("" : "=r"(result) : "0"(result));
    return result;
}
}
