#include "fft/event_attack.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Thread body: step the deployment roster cursor to the previous deployable
 * unit, wrapping over the 20 slots (or jump to the pending index when one is
 * set), load that unit into the panels and request their redraw with slide
 * direction 2. */
void attack_deploy_select_previous_roster_unit(void) {
    s32 pending;

    attack_deploy_mark_other_squad_units_undeployable();
    pending = g_attack_deploy_pending_roster_index;
    if (pending != -1) {
        g_attack_deploy_selected_roster_index = pending;
        g_attack_deploy_pending_roster_index = -1;
    } else {
        do {
            g_attack_deploy_selected_roster_index--;
            if (g_attack_deploy_selected_roster_index < 0) {
                g_attack_deploy_selected_roster_index = 19;
            }
        } while (g_attack_deploy_roster_unit_deployable[g_attack_deploy_selected_roster_index] == 0);
    }
    attack_editor_load_party_unit(1, g_attack_deploy_selected_roster_index);
    g_attack_deploy_roster_navigation_direction = 2;
    g_attack_status_display_thread_params = 0;
    g_attack_numeric_editor_redraw_request = 1;
    g_attack_character_status_redraw_request = 1;
    g_attack_deploy_selected_unit_deployed
        = attack_deploy_is_roster_unit_deployed(g_attack_deploy_selected_roster_index);
    thread_yield();
    thread_exit_current();
}
