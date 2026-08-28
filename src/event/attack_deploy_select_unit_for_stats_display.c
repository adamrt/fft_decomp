#include "fft/attack.h"
#include "psx/types.h"

void attack_deploy_select_unit_for_stats_display(void) {
    attack_deploy_mark_other_squad_units_undeployable();
    while (1) {
        if (g_attack_deploy_selected_roster_index >= 20) {
            g_attack_deploy_selected_roster_index = 0;
        }
        if (g_attack_deploy_roster_unit_deployable[g_attack_deploy_selected_roster_index] != 0) {
            break;
        }
        g_attack_deploy_selected_roster_index++;
    }
    g_attack_status_display_thread_params = 0;
    attack_load_party_unit_into_editor(1, g_attack_deploy_selected_roster_index);
}
