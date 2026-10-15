#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Leave only the action's target tile marked as targeted. Returns 1, or -1
 * when that tile is off the map or blocked. */
s32 target_disable_green_panel_on_all_but_target_tile(const action_command_t* action) {
    action_command_t action_copy;
    u8 x;
    u8 y;
    u8 level;
    map_tile_t* tile;

    main_util_copy_action_data(action, &action_copy);
    /* Retail reads only the low byte of each 16-bit coordinate. */
    x = (u8)action_copy.target_position.vx;
    y = (u8)action_copy.target_position.vz;
    level = (u8)action_copy.target_position.vy;
    if (x < g_map_max_x) {
        if (y < g_map_max_y) {
            if (level < 2) {
                tile = &g_map_tile_data[(level << 8) + y * g_map_max_x + x];
                if ((tile->flags.blocked) == 0) {
                    target_disable_green_panel_flags();
                    tile->ceiling_depth_and_marks.targeted = 1;
                    return 1;
                }
            }
        }
    }
    return -1;
}
