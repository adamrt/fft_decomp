#include "fft/event_attack.h"
#include "psx/types.h"

void attack_deploy_remove_roster_unit_from_tiles(s32 roster_id) {
    s32 row;
    s32 column;

    for (row = 0; row < 5; row++) {
        for (column = 0; column < 5; column++) {
            if (g_attack_deploy_roster_id_by_tile[row][column] == roster_id) {
                g_attack_deploy_roster_id_by_tile[row][column] = ATTACK_DEPLOY_ROSTER_ID_NONE;
                return;
            }
        }
    }
}
