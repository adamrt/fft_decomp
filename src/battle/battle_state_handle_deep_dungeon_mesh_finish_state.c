#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "psx/types.h"

/* DeepDungeonMeshFinish game-state handler: when the Deep Dungeon map has loaded,
 * enter the after-command state. */
void battle_state_handle_deep_dungeon_mesh_finish_state(void) {
    if (map_update_deep_dungeon_and_animation() != 0) {
        battle_state_enter_after_command();
    }
}
