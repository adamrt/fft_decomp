#include "fft/event_attack.h"
#include "psx/types.h"

void attack_deploy_mark_other_squad_units_undeployable(void) {
    s32 squad_id;
    s32 empty;
    s32 unit_id;
    u8* squad;
    u8* entry;

    battle_copy_bytes(g_attack_deploy_roster_unit_deployable, g_attack_deploy_unit_fieldable, 20);
    squad_id = 0;
    empty = 0xff;
    squad = g_attack_deploy_units_by_squad[0];
    do {
        if (squad_id != g_attack_deploy_current_squad) {
            entry = squad;
            do {
                unit_id = *entry++;
                if (unit_id != empty) {
                    g_attack_deploy_roster_unit_deployable[unit_id] = 0;
                }
            } while ((s32)entry < (s32)(squad + 25));
        }
        squad_id++;
        squad += 25;
    } while (squad_id < 4);
}
