#include "fft/battle.h"
#include "psx/types.h"

void battle_menu_refresh_hovered_unit_stats_display(void) {
    s16* display_mode;

    battle_menu_store_units_small_in_battle_display_data();
    display_mode = &g_battle_menu_hovered_unit_display_mode;
    *display_mode = 2;
    if (g_battle_menu_active_turn_banner.list_index < 0) {
        *display_mode = 0x0c00;
    }
    battle_menu_display_hovered_unit_stats(&g_battle_menu_hovered_unit_stats_display, 0,
        (menu_number_entry_t*)((u8*)display_mode - 0x50), &g_battle_menu_active_turn_banner);
}
