#include "fft/battle.h"

void* battle_effect_get_file_pointer(s16 effect_id) {
    return g_battle_effect_file_ptrs[effect_id];
}
