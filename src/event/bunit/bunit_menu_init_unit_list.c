#include "fft/event_bunit.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "psx/types.h"

/* Set up the unit list page: an empty scrollable list, the row callbacks for
 * current and maximum stats, level, experience, Brave and Faith, and the
 * status banner's position (at the bottom, y 0x90, while the selected unit
 * is in the upper rows). The argument is not read. */
void bunit_menu_init_unit_list(s32 unused_selected_index) {
    s32 ret;

    bunit_menu_init_scrollable_list_core(0, 0, 0);
    g_menu_row_callbacks[0].value = bunit_unit_get_current_stat;
    g_menu_row_callbacks[1].value = bunit_unit_get_max_stat;
    g_menu_row_callbacks[4].value = bunit_unit_get_level;
    g_menu_row_callbacks[5].value = bunit_unit_get_experience;
    g_menu_row_callbacks[6].value = bunit_unit_get_brave;
    g_menu_row_callbacks[7].value = bunit_unit_get_faith;
    menu_set_vsync_mode_and_event_speed(0);
    ret = bunit_menu_is_selected_unit_in_upper_rows(g_bunit_unit_list_page_offset);
    g_bunit_status_banner_at_bottom = ret;
    g_bunit_status_banner_at_bottom_latch = ret;
    if (g_bunit_status_banner_at_bottom != 0) {
        g_bunit_status_display_offset_y = 0x90;
    } else {
        g_bunit_status_display_offset_y = 0;
    }
    bunit_gfx_init_rhombus_cursor_tpages();
}
