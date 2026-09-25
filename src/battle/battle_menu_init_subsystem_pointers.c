#include "fft/battle.h"
#include "psx/types.h"

/* Rebind the BATTLE menu subsystem's working pointers and clear its
 * per-session counters. Called first by battle_menu_init_subsystems. */
void battle_menu_init_subsystem_pointers(void) {
    g_battle_threads = g_battle_thread_contexts;
    g_text_glyph_bitmap_data = g_battle_text_glyph_bitmaps;
    g_battle_menu_glyph_image = g_battle_menu_glyph_image_data;
    g_battle_event_block = g_event_script_buffer;
    g_battle_script_variables = g_main_script_variables;
    g_battle_menu_thread_menu_data = g_battle_menu_idle_action_entries;
    g_battle_help_text_id_tables_ptr = -1;
    g_battle_unit_graphics_load_pending = 0;
    g_battle_script_threads_idle = 0;
    g_battle_script_frame_result_override = 0;
}
