#include "fft/battle_ai.h"

/*
 * Fill target-distance estimates for rows containing reachable tiles.
 *
 * Unmounted Teleport or Teleport 2 uses Manhattan distance; other movement subtracts
 * propagated remaining-range values. Fill every map-width tile in each
 * selected row, leaving zero-mask rows unchanged. Return -1 when the child
 * suspends through its out-parameter, or 0 after filling the grid.
 */
s32 battle_ai_fill_target_distance_grid(battle_ai_coords_t* target) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 suspended = 0;
    s32 use_manhattan;
    s32 target_remaining_range;
    s32 level, x, y;
    s32 distance, y_distance;

    if (g_battle_ai_data_base.decision_state != 0)
        g_battle_ai_data_base.search_resume_flag = 0;
    else
        g_battle_ai_data_base.search_resume_flag = 1;
    use_manhattan = 0;
    if (!(ai->acting_unit->mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER)
        && (ai->acting_unit->movement_abilities[1]
            & (BATTLE_MOVEMENT_SET_2_TELEPORT | BATTLE_MOVEMENT_SET_2_TELEPORT_2))) {
        use_manhattan = 1;
    } else {
        battle_ai_propagate_target_movement(ai->acting_unit_id, target->bytes.x, target->bytes.y,
            target->bytes.elevation, ai->search_resume_flag, &suspended, 1);
        if (suspended != 0)
            return -1;
        ai->decision_state = 0;
        target_remaining_range
            = g_battle_target_panels[(target->bytes.elevation << 8) + target->bytes.y * ai->map_max_x + target->bytes.x]
                  .remaining_range;
    }
    for (level = 0; level < 2; level++) {
        for (y = 0; y < ai->map_max_y; y++) {
            if (ai->reachable_tiles[0][level][y] != 0) {
                for (x = 0; x < ai->map_max_x; x++) {
                    if (use_manhattan) {
                        distance = target->bytes.x - x;
                        if (distance < 0)
                            distance = -distance;
                        y_distance = target->bytes.y - y;
                        if (y_distance < 0)
                            y_distance = -y_distance;
                        distance += y_distance;
                    } else {
                        distance = target_remaining_range
                            - g_battle_target_panels[(level << 8) + y * ai->map_max_x + x].remaining_range;
                    }
                    ai->tile_target_distance[level][y][x] = distance;
                }
            }
        }
    }
    return 0;
}
