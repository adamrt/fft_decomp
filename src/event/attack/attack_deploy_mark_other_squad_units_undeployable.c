#include "fft/event_attack.h"
#include "fft/memory.h"
#include "psx/types.h"

/* Rebuild the "deployable" flags of the 20 roster slots for the current
 * squad: every fieldable unit, minus the units already placed in one of the
 * other three squads. */
void attack_deploy_mark_other_squad_units_undeployable(void) {
    s32 squad_id;
    s32 i;
    s32 unit_id;

    mem_copy_bytes_nonempty(
        g_attack_deploy_roster_unit_deployable, g_attack_deploy_unit_fieldable, PARTY_ROSTER_SLOT_COUNT);
    for (squad_id = 0; squad_id < 4; squad_id++) {
        if (squad_id != g_attack_deploy_current_squad) {
            for (i = 0; i < 25; i++) {
                unit_id = g_attack_deploy_units_by_squad[squad_id][i];
                if (unit_id != ATTACK_DEPLOY_ROSTER_ID_NONE) {
                    g_attack_deploy_roster_unit_deployable[unit_id] = 0;
                }
            }
        }
    }
}
