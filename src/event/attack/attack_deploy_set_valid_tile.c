#include "fft/event_attack.h"
#include "psx/types.h"

void attack_deploy_set_valid_tile(s32 column, s32 row, u8 is_valid) {
    g_attack_deploy_valid_tiles[row][column] = is_valid;
}
