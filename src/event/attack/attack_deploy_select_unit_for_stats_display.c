#include "fft/event_attack.h"

/* Move the roster cursor forward (wrapping at 20) to the first unit the
 * current squad may still deploy and load it into the status panels. */
void attack_deploy_select_unit_for_stats_display(void) {
    attack_deploy_mark_other_squad_units_undeployable();
    while (1) {
        if (g_attack_deploy_selected_roster_index >= PARTY_ROSTER_SLOT_COUNT) {
            g_attack_deploy_selected_roster_index = 0;
        }
        if (g_attack_deploy_roster_unit_deployable[g_attack_deploy_selected_roster_index] != 0) {
            break;
        }
        g_attack_deploy_selected_roster_index++;
    }
    g_attack_status_display_thread_params = 0;
    attack_editor_load_party_unit(1, g_attack_deploy_selected_roster_index);
}
