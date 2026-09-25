#include "fft/wldcore.h"
#include "psx/pad.h"

/* Provisional: the yes/no confirmation menu level this handler drives. The
 * level owns a cursor window (0x00), a render record (0x04), the proposition
 * it acts on (0x10), the highlighted answer (0x14) and a second window
 * (0x20). Only these words are touched here. */
typedef struct wldcore_menu_proposition_confirm_level {
    s32 window;        /* 0x00 */
    s32 render_index;  /* 0x04 */
    u8 _unused_08[8];  /* 0x08 */
    s32 proposition;   /* 0x10; index into g_main_active_propositions */
    s32 choice;        /* 0x14; 0 = first row, 1 = second row */
    s32 confirmed;     /* 0x18; set once the answer has been taken */
    u8 _unused_1c[4];  /* 0x1c */
    s32 second_window; /* 0x20 */
} wldcore_menu_proposition_confirm_level_t;

/* Input step for the two-row proposition confirmation window: up/down toggle
 * the highlighted row and move the cursor window, Circle either
 * accepts the proposition and starts WORLD thread 14 with text 0xb81e or
 * closes the level, and Cross closes it. */
void wldcore_proposition_handle_cancel_confirm_input(wldcore_menu_proposition_confirm_level_t* level) {
    s32 render_index;
    s32 window;
    s32 choice;
    s32 row_y;
    s32 y;

    if (g_wldcore_window_render_records[level->render_index].flags & 0x100) {
        return;
    }
    if (level->confirmed != 0) {
        if (g_wldcore_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
            goto close;
        }
        return;
    }
    if (g_wldcore_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        /* Shared tail: the target keeps this copy and the other close paths
         * jump into it; duplicated copies cross-jump into the last one. */
    close:
        g_wldcore_window_record_count -= 2;
        g_wldcore_window_render_record_count -= 2;
        g_wldcore_window_render_object_count -= 4;
        g_wldcore_menu_stack_depth--;
        wldcore_list_open_propositions();
        return;
    }
    if (wldcore_input_check_repeating_directional(PSX_PAD_UP) != 0
        || wldcore_input_check_repeating_directional(PSX_PAD_DOWN) != 0) {
        choice = level->choice;
        choice = choice ^ 1;
        window = level->window;
        render_index = level->render_index;
        level->choice = choice;
        g_wldcore_window_records[window].y
            = (y = g_wldcore_window_render_records[render_index].base_y) + (row_y = choice * 16 + 14);
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    }
    if (!(g_wldcore_new_button_presses & PSX_PAD_CIRCLE)) {
        return;
    }
    if (level->confirmed != 0) {
        return;
    }
    wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
    if (level->choice == 1) {
        goto close;
    }
    world_thread_set_parameters(0xE, 0x19, 0xB81E, 0);
    level->confirmed = 1;
    g_main_active_propositions[level->proposition].flags |= 2;
    g_wldcore_window_records[level->window].flags |= 0x10;
    g_wldcore_window_records[level->second_window].flags |= 0x10;
    g_wldcore_window_render_records[level->render_index].flags |= 0x10;
}
