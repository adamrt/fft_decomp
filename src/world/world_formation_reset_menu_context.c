#include "fft/menu.h"
#include "fft/world.h"
#include "psx/libgpu.h"

/* Resets shared context before framebuffer setup and after formation or
 * memory-card sessions. */
void world_formation_reset_menu_context(void) {
    DrawSync(0);
    world_menu_init_subsystem_pointers();
    world_menu_reset_subsystems();
    text_relocate_pointer_table(g_world_menu_text_file_buffer);
    world_text_generate_formation_unit_name_string();
    DrawSync(0);
    g_menu_thread_menu_data = g_world_menu_default_entries;
    g_text_section_pointers[TEXT_SECTION_LOCATION_NAMES] = g_world_location_names_resident;
    g_menu_text_palette_offset = 0;
}
