#include "fft/battle_ai.h"
#include "fft/unit_slots.h"

/*
 * Score reachable tiles by nearby eligible enemies.
 *
 * Each enemy contributes max(10 - Manhattan distance, 0), ignoring height.
 * Only scenario-0 reachable tiles are updated; other cells remain unchanged.
 * The shared targetability list is rebuilt before scoring both map layers.
 */
void battle_ai_fill_foe_proximity_grid(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 i;
    s32 level;
    s32 y;
    s32 x;
    s32 distance;
    s32 dy;
    u16 row;
    battle_stats_t* unit;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        ai->targetability.live.unit_targetable[i] = 0;
        if (battle_ai_check_target_type(i) == 0 && battle_ai_should_exclude_from_imminent_enemies(i) == 0) {
            ai->targetability.live.unit_targetable[i] = 1;
        }
    }
    for (level = 0; level < 2; level++) {
        battle_stats_t* units = g_battle_unit_stats;
        s32 proximity_limit = 10;
        for (y = 0; y < ai->map_max_y; y++) {
            row = ai->reachable_tiles[0][level][y];
            if (row == 0) {
                continue;
            }
            for (x = 0; x < ai->map_max_x; x++) {
                if (!((row << x) & 0x8000)) {
                    continue;
                }
                ai->tile_foe_proximity[level][y][x] = 0;
                for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                    unit = &units[i];
                    if (ai->targetability.live.unit_targetable[i] == 0) {
                        continue;
                    }
                    distance = unit->x - x;
                    if (distance < 0) {
                        distance = -distance;
                    }
                    dy = unit->position.bits.y - y;
                    if (dy < 0) {
                        dy = -dy;
                    }
                    distance += dy;
                    if (distance < 11) {
                        distance = proximity_limit - distance;
                        ai->tile_foe_proximity[level][y][x] += distance;
                    }
                }
            }
        }
    }
}
