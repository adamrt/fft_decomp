/* Menu palette colors and companion data file load. */

#include "fft/battle_menu.h"
#include "fft/battle_text.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Copies the normal and the dimmed 16-color menu palette into the menu palette color table. */
void menu_set_palette_colors(const menu_palette_pair_t* source) {
    rgba5551_t* destination;

    destination = g_menu_palette_colors.colors;
    mem_copy_bytes_nonempty(destination, source->colors, sizeof(palette_t));
    mem_copy_bytes_nonempty(destination + PALETTE_COLOR_COUNT, source->colors + PALETTE_COLOR_COUNT, sizeof(palette_t));
}

void menu_init_palette_and_text(s32 unused_0) {
    menu_set_palette_colors(&g_menu_default_palette_colors);
    text_init_section_pointers_variant();
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * menu_init_palette_and_text and menu_open_companion_data_file. Nothing on the
 * disc calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
void menu_noop_4(void) {
}

/* Non-blocking two-step file load driven by g_thread_call_target:
 * issue the read for the pending request, then poll until it completes and
 * clear the request. */
void menu_open_companion_data_file(void) {
    if ((g_menu_companion_data_file_request_index != 0) && (g_menu_companion_data_file_loading == 0)) {
        g_thread_call_target = (void (*)(void))main_file_call_build_header;
        if (thread_call_on_main_stack(g_menu_companion_data_file_lbas[g_menu_companion_data_file_request_index],
                g_menu_companion_data_file_sizes[g_menu_companion_data_file_request_index],
                g_menu_companion_data_file_buffer)
            == 0) {
            g_menu_companion_data_file_loading = 1;
        }
    } else if ((g_menu_companion_data_file_request_index != 0) && (g_menu_companion_data_file_loading != 0)) {
        g_thread_call_target = main_file_is_still_loading;
        if (thread_call_on_main_stack() == 0) {
            g_menu_companion_data_file_loading = 0;
            g_menu_companion_data_file_request_index = 0;
        }
    }
}
