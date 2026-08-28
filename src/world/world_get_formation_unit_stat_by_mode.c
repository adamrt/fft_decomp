#include "fft/data.h"
#include "fft/world.h"
#include "psx/types.h"

/* Pick the formation unit's stat by the current stat-select mode. */
s32 world_get_formation_unit_stat_by_mode(s32 unit_index, s32 fallback) {
    if (g_world_formation_stat_display_mode == 0) {
        fallback = g_world_formation_unit_pointers[unit_index]->hp;
    } else if (g_world_formation_stat_display_mode == 1) {
        fallback = g_world_formation_unit_pointers[unit_index]->mp;
    } else if (g_world_formation_stat_display_mode == 2) {
        fallback = 0x20000064;
    }
    return fallback;
}
