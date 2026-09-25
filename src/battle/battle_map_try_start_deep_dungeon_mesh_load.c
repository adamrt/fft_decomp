#include "fft/battle.h"

/* Target 0x80070ddc. */
s32 battle_map_try_start_deep_dungeon_mesh_load(void) {
    if (battle_map_check_deep_dungeon_crystal_and_init_state() != 0) {
        g_battle_game_state = BATTLE_GAME_STATE_DEEP_DUNGEON_MESH_LOAD;
        g_animation_speed = 1;
        return 1;
    }

    return 0;
}
