#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Resets shared context before framebuffer setup and after formation or
 * memory-card sessions. */
void world_formation_reset_menu_context(void) {
    DrawSync(0);
    world_init_scene_bindings();
    world_menu_reset_runtime();
    world_text_init_format_section_pointers(g_world_menu_text_file_buffer);
    world_text_generate_formation_unit_name_string();
    DrawSync(0);
    g_world_menu_thread_menu_data = g_world_menu_default_entries;
    g_world_text_location_names = g_world_location_names_resident;
    g_world_menu_text_color = 0;
}
