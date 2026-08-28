#include "psx/types.h"

void battle_state_handle_deep_dungeon_mesh_finish_state(void) {
    if (battle_map_update_deep_dungeon_and_animation() != 0) {
        battle_state_enter_after_command();
    }
}
