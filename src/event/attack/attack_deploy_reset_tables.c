#include "fft/event_attack.h"
#include "psx/types.h"

/* Empty the deployment tables: the 5x5 grid of roster ids by tile and the
 * four squads' 25-entry unit lists. */
void attack_deploy_reset_tables(void) {
    s32 row;
    s32 column;

    row = 0;
    do {
        column = 4;
        do {
            g_attack_deploy_roster_id_by_tile[row][column] = ATTACK_DEPLOY_ROSTER_ID_NONE;
            column--;
        } while (column >= 0);
        row++;
    } while (row < 5);

    row = 0;
    do {
        column = 24;
        do {
            g_attack_deploy_units_by_squad[row][column] = ATTACK_DEPLOY_ROSTER_ID_NONE;
            column--;
        } while (column >= 0);
        row++;
    } while (row < 4);
}
