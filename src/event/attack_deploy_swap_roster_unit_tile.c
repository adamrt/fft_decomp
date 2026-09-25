#include "fft/event_attack.h"
#include "psx/types.h"

void attack_deploy_swap_roster_unit_tile(s32 column, s32 row, s32 roster_id) {
    s32 scan_row;
    s32 scan_column;
    s32 old_value;
    for (scan_row = 0; scan_row < 5; scan_row++) {
        for (scan_column = 0; scan_column < 5; scan_column++) {
            old_value = g_attack_deploy_roster_id_by_tile[scan_row][scan_column];
            if (old_value == roster_id) {
                g_attack_deploy_roster_id_by_tile[scan_row][scan_column]
                    = g_attack_deploy_roster_id_by_tile[row][column];
                g_attack_deploy_roster_id_by_tile[row][column] = old_value;
                return;
            }
        }
    }
}
