#include "fft/world.h"

/* Fill four caller buffers from the default tables. */
void world_gfx_copy_screen_setup_out(void* dest0, void* dest1, void* dest2, void* dest3) {
    world_script_copy_bytes(dest0, &g_world_unit_selected_status_billboard, 0x22);
    world_script_copy_bytes(dest1, g_world_active_unit_data, 0xe);
    world_script_copy_bytes(dest2, g_world_unit_editor_fields, 0x40);
    world_script_copy_bytes(dest3, &g_world_map_selected_tile_data, 0xa);
}
