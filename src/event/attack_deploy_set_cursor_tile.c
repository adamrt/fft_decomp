#include "fft/attack.h"
#include "psx/types.h"

void attack_deploy_set_cursor_tile(s32 column, s32 row) {
    g_attack_deploy_cursor_column[0] = column;
    g_attack_deploy_cursor_row[0] = row;
}
