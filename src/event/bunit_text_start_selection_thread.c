#include "fft/battle.h"
#include "fft/battle_text.h"
#include "fft/bunit.h"
#include "psx/types.h"

/* Start the text thread for g_bunit_text_selection_id (ids above 0x1FFFF are
 * stashed in g_bunit_help_screen_id instead); menu_state + 0x38 is the saved selection. */
void bunit_text_start_selection_thread(u8* menu_state) {
    if (g_bunit_text_selection_id > 0x1FFFF) {
        g_bunit_help_screen_id = g_bunit_text_selection_id;
        return;
    }
    if (g_bunit_text_selection_id <= 0) {
        return;
    }
    battle_text_save_pointer_table();
    battle_text_relocate_pointer_table((const u32*)&g_bunit_help_text_table);
    battle_thread_start(1, battle_text_character_handling_thread);
    battle_thread_set_parameters(1, menu_state + 0x38, g_bunit_text_selection_id, 0);
    g_event_mode = 1;
}
