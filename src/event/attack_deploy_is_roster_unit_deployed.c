#include "fft/attack.h"
#include "psx/types.h"

s32 attack_deploy_is_roster_unit_deployed(s32 roster_id) {
    s32 row;
    s32 column;

    for (row = 0; row < 5; row++) {
        for (column = 0; column < 5; column++) {
            if (g_attack_deploy_roster_id_by_tile[row][column] == roster_id) {
                return 1;
            }
        }
    }
    return 0;
}
