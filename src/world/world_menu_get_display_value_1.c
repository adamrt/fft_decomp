#include "fft/world.h"
#include "psx/types.h"

s32 world_menu_get_display_value_1(void) {
    s32 value = g_world_selected_unit_stat_summary.hp_bonus;
    if (value == 0) {
        return 0x20000000;
    }
    return value;
}
