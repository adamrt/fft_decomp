#include "fft/bunit.h"
#include "psx/types.h"
void bunit_unit_copy_selected_data(void) {
    /* The retail code reads the index unsigned here (lhu) and signed below (lh). */
    s32 idx = *(u16*)&g_bunit_unit_selected_index;
    u8* unit_data = (u8*)g_bunit_unit_data[(s16)idx];
    g_bunit_unit_comparison_index = idx;
    bcopy(unit_data, &g_bunit_panel_selected_billboard, 0x22);
    bcopy((u8*)g_bunit_unit_data[g_bunit_unit_selected_index] + 0x22, g_bunit_panel_selected_unit_data, 0xE);
    bcopy((u8*)g_bunit_unit_data[g_bunit_unit_selected_index] + 0x30, &g_bunit_editor_unit_fields, 0x40);
    g_bunit_status_display_redraw_request = 1;
    g_bunit_comparison_display_redraw_request = 1;
    g_bunit_character_status_redraw_request = 1;
    g_bunit_numeric_editor_redraw_request = 1;
}
