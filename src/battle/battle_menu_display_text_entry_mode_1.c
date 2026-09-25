#include "fft/battle.h"
#include "psx/types.h"

/* Target 0x8014b320. */
void battle_menu_display_text_entry_mode_1(s32 text_id, s32 image, s32 origin) {
    g_battle_thread_call_target = (void (*)(void))battle_menu_display_text;
    battle_thread_call_on_main_stack(text_id, image, origin, 1);
}
