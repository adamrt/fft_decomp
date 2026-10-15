#include "fft/event_bunit.h"
/* Copy the selected unit's stat summary, identity and stat detail blocks
 * into the panel records and request a redraw of all four panels. */
void bunit_unit_copy_selected_data(void) {
    bunit_unit_t* comparison;

    g_bunit_unit_comparison_index = g_unit_selected_index;
    comparison = g_bunit_unit_pointers[g_bunit_unit_comparison_index];
    bcopy(comparison, &g_panel_selected_billboard, 0x22);
    bcopy(&g_bunit_unit_pointers[g_unit_selected_index]->name_index, g_bunit_panel_selected_unit_data, 0xE);
    bcopy(&g_bunit_unit_pointers[g_unit_selected_index]->move, &g_editor_unit_fields, 0x40);
    g_status_display_redraw_request = 1;
    g_bunit_comparison_display_redraw_request = 1;
    g_bunit_character_status_frame_config.redraw_request = 1;
    g_bunit_numeric_editor_thread_params.redraw_request = 1;
}
