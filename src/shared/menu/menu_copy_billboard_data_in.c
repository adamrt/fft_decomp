#include "fft/memory.h"
#include "fft/menu.h"

/* Load the active-turn banner, the active-turn unit record and the billboard data
 * from the caller's copies: the inverse of menu_copy_billboard_data_out,
 * without the cursor tile record. REQUIRE.OUT publishes its own records this way. */
void menu_copy_billboard_data_in(const menu_unit_status_record_t* banner, const menu_active_unit_t* unit,
    const menu_status_panel_slot_storage_t* billboard) {
    mem_copy_bytes_nonempty(&g_menu_selected_unit_status, banner, sizeof(g_menu_selected_unit_status));
    mem_copy_bytes_nonempty(&g_menu_selected_unit, unit, sizeof(g_menu_selected_unit));
    mem_copy_bytes_nonempty(&g_menu_selected_unit_fields, billboard, sizeof(g_menu_selected_unit_fields));
}
