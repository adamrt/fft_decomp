#include "fft/battle.h"
#include "psx/types.h"

void battle_state_handle_deep_dungeon_mesh_load_state(void) {
    if (battle_map_update_deep_dungeon_and_animation() != 0) {
        battle_action_check_between_turn_events();
    }
}
