#include "fft/bunit.h"
#include "psx/types.h"

void bunit_menu_init_unit_list(void) {
    s32 ret;

    bunit_menu_init_scrollable_list_core((s16*)0, 0, 0);
    g_bunit_menu_row_callbacks[0] = (bunit_menu_row_callback_t)bunit_unit_get_current_stat;
    g_bunit_menu_row_callbacks[1] = (bunit_menu_row_callback_t)bunit_unit_get_max_stat;
    g_bunit_menu_row_callbacks[4] = bunit_unit_get_level;
    g_bunit_menu_row_callbacks[5] = bunit_unit_get_experience;
    g_bunit_menu_row_callbacks[6] = bunit_unit_get_brave;
    g_bunit_menu_row_callbacks[7] = bunit_unit_get_faith;
    bunit_menu_set_event_speed(0);
    ret = bunit_menu_is_unit_list_row_visible(g_bunit_unit_list_page_offset);
    g_bunit_status_banner_at_bottom = ret;
    g_bunit_status_banner_at_bottom_latch = ret;
    if (g_bunit_status_banner_at_bottom != 0) {
        g_bunit_status_display_offset_y = 0x90;
    } else {
        g_bunit_status_display_offset_y = 0;
    }
    bunit_gfx_init_rhombus_cursor_tpages();
}
