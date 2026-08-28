#include "fft/world.h"
#include "psx/types.h"

s32 world_menu_get_value_1(void) {
    return g_world_selected_unit_stat_summary.hp_bonus;
}
