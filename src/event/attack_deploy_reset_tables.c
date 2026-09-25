#include "fft/event_attack.h"
#include "psx/types.h"

void attack_deploy_reset_tables(void) {
    s32 row;
    s32 column;

    row = 0;
    do {
        column = 4;
        do {
            g_attack_deploy_roster_id_by_tile[row][column] = 0xff;
            column--;
        } while (column >= 0);
        row++;
    } while (row < 5);

    row = 0;
    do {
        column = 24;
        do {
            g_attack_deploy_units_by_squad[row][column] = 0xff;
            column--;
        } while (column >= 0);
        row++;
    } while (row < 4);
}
