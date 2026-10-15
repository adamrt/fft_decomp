#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/battle_text.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"

/* Bind the scene working pointers and clear its menu-session flags. */
void menu_init_subsystem_pointers(void) {
    g_threads = g_thread_contexts;
    g_text_glyph_bitmap_data = g_text_glyph_bitmaps;
    g_menu_glyph_sheet = &g_menu_glyph_image_data;
    g_script_event_block = g_main_script_event_block;
    g_script_variables = g_main_script_variables;
    g_menu_thread_menu_data = g_menu_idle_action_entries;
    g_menu_help_active_text_id_tables = (u16* const*)-1;
    g_unit_graphics_load_pending = 0;
    g_script_threads_idle = 0;
    g_script_frame_result_override = 0;
}
