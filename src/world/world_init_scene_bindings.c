#include "fft/event.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

extern s32 g_main_script_variables[0x100];

/* Bind the WORLD scheduler, script and menu subsystems to their backing
   storage and clear the per-scene state they share. */
void world_init_scene_bindings(void) {
    g_world_threads = g_world_thread_contexts;
    g_world_text_glyph_bitmap_data = g_world_text_glyph_bitmaps;
    g_world_menu_glyph_sheet = g_world_menu_glyph_image;
    g_world_event_script = (const u8*)g_event_script_buffer;
    g_world_script_variables = g_main_script_variables;
    g_world_menu_thread_menu_data = g_world_menu_system_entries;
    g_world_help_text_id_tables_ptr = -1;
    g_world_unit_graphics_load_pending = 0;
    g_world_event_finish_check_pending = 0;
    g_world_frame_result_override = 0;
}
