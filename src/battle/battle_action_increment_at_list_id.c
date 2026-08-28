#include "fft/battle.h"
#include "psx/types.h"

void battle_action_increment_at_list_id(void) {
    s32 id = g_battle_action_at_list_id + 1;

    g_battle_action_at_list_id = id;
    if (id >= 0x28) {
        g_battle_action_at_list_id = 0;
    }
}
