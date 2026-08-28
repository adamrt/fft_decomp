#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_proposition_calculate_base_jp_and_gil(void) {
    u16* first;
    u16* second;
    s32 count;
    s32 i;
    s32 value;

    if (g_wldcore_job_selection.gate == 0) {
        first = (u16*)wldcore_proposition_get_data_pointer(8);
        second = (u16*)wldcore_proposition_get_data_pointer(9);
        count = g_main_active_propositions[g_wldcore_job_selection.proposition_index].participant_count;
        value = 0;

        for (i = 0; i < count; i++) {
            value = first[g_wldcore_selected_proposition_row[0].fields.jp_reward - 1];
            if (g_wldcore_job_selection.result == 1) {
                value = value / 10;
            }
            if (g_wldcore_job_selection.result == 2) {
                value = value / 20;
            }
            g_wldcore_job_selection.rows[0][i] = value;
            g_wldcore_job_selection.rows[0][i] += g_wldcore_job_selection.rows[1][i];
        }

        for (i = 0; i < count; i++) {
            value = second[g_wldcore_selected_proposition_row[0].fields.gil_reward - 1];
            if (g_wldcore_job_selection.result == 1) {
                value = value / 5;
            }
            if (g_wldcore_job_selection.result == 2) {
                value = value / 20;
            }
            g_wldcore_job_selection.rows[2][i] = value;
            g_wldcore_job_selection.rows[2][i] += g_wldcore_job_selection.rows[1][i];
        }
    }
}
