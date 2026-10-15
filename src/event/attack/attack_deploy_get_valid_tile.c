#include "fft/event_attack.h"
#include "psx/types.h"

u8 attack_deploy_get_valid_tile(s32 column, s32 row) {
    return g_attack_deploy_valid_tiles[row][column];
}
