#include "fft/battle.h"
#include "psx/types.h"

void battle_script_set_current_otag_entry_and_update_input(void* entry, s32 input) {
    battle_script_update_event_input_state(input);
    g_current_otag_entry = entry;
    battle_menu_draw_active_window_frames();
}
