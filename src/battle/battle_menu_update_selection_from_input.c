#include "fft/battle.h"
#include "psx/pad.h"

void battle_menu_update_selection_from_input(battle_menu_idle_action_entry_t* ctx, s32* sel) {
    s32 selection;
    s32 max;
    s32 cur;

    if (battle_menu_can_accept_input() != 0) {
        if (g_battle_script_event_input & PSX_PAD_UP) {
            selection = *sel;
            if (selection == 0) {
                selection = ctx->last_option;
            } else {
                selection = selection - 1;
            }
            *sel = selection;
            battle_sound_set_effect_to_cursor_move();
        }
        if (g_battle_script_event_input & PSX_PAD_DOWN) {
            max = ctx->last_option;
            cur = *sel;
            if (cur == max) {
                *sel = 0;
            } else {
                *sel = cur + 1;
            }
            battle_sound_set_effect_to_cursor_move();
        }
    }
}
