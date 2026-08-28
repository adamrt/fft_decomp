#include "fft/battle.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

s32 battle_map_refresh_deep_dungeon(void) {
    if (battle_map_check_deep_dungeon_crystal_and_init_state()) {
        g_battle_game_state = BATTLE_GAME_STATE_DEEP_DUNGEON_MESH_FINISH;
        g_animation_speed = 1;
        return 1;
    }
    return 0;
}
