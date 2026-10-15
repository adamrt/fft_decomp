#include "fft/battle.h"
#include "fft/battle_map.h"
#include "psx/types.h"

/* As map_try_start_deep_dungeon_mesh_load, entering the mesh-finish state
 * instead: the crystal count check made when a unit's post-movement steps end. */
s32 map_refresh_deep_dungeon(void) {
    if (map_check_deep_dungeon_crystal_and_init_state()) {
        g_battle_game_state = BATTLE_GAME_STATE_DEEP_DUNGEON_MESH_FINISH;
        g_main_system_animation_speed = 1;
        return 1;
    }
    return 0;
}
