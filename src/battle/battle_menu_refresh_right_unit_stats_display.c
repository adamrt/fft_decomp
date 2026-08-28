#include "fft/battle.h"
#include "psx/types.h"

void battle_menu_refresh_right_unit_stats_display(void) {
    s16* display_mode;

    battle_menu_store_units_small_in_battle_display_data();
    display_mode = &g_battle_menu_right_unit_display_mode;
    *display_mode = 2;
    if (g_battle_menu_status_billboard.list_index < 0) {
        *display_mode = 0x0c00;
    }
    battle_menu_display_hovered_unit_stats(&g_battle_menu_right_unit_stats_display, 3,
        (menu_number_entry_t*)((u8*)display_mode - 0x50), &g_battle_menu_status_billboard);
}
