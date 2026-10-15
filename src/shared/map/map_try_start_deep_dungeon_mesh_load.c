#include "fft/battle.h"
#include "fft/battle_map.h"

/* Between turns: when the crystal count calls for a map reload
 * (map_check_deep_dungeon_crystal_and_init_state), enter the Deep Dungeon
 * mesh-load state at animation speed 1. Returns 1 when the state changed. */
s32 map_try_start_deep_dungeon_mesh_load(void) {
    if (map_check_deep_dungeon_crystal_and_init_state() != 0) {
        g_battle_game_state = BATTLE_GAME_STATE_DEEP_DUNGEON_MESH_LOAD;
        g_main_system_animation_speed = 1;
        return 1;
    }

    return 0;
}
