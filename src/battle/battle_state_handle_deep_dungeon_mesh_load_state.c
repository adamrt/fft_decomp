#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "psx/types.h"

/* padding */

/* DeepDungeonMeshLoad game-state handler: when the Deep Dungeon map has loaded,
 * run the between-turn events. */
void battle_state_handle_deep_dungeon_mesh_load_state(void) {
    if (map_update_deep_dungeon_and_animation() != 0) {
        action_check_between_turn_events();
    }
}
