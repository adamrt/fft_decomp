#include "fft/open.h"
#include "psx/pad.h"

/* Per-frame handler of the sound type menu (handler 3).
 *
 * Circle or Start applies the highlighted mode (mono, stereo or wide) with
 * suzuki_sound_set_type and stores it in the game options; Cross leaves the mode as
 * it is. Both set step 2, and the next update releases the three option records,
 * pops the controller, shows the title menu's four entries again and zeroes the
 * title controller's idle timer. Up, and Down or Select, move the highlight and
 * wrap around. */
void open_menu_handle_sound_type_input(open_controller_record_t* record) {
    open_sound_menu_state_t* menu = &record->sound_menu;
    s32 render_record;

    if (menu->step == 2) {
        g_open_gfx_next_render_record_36 -= OPEN_SOUND_MENU_ENTRY_COUNT;
        g_open_gfx_render_record_pointer_count -= OPEN_SOUND_MENU_ENTRY_COUNT;
        g_open_current_controller_index -= 1;
        open_gfx_show_four_render_records(&g_open_controller_records[g_open_current_controller_index].title);
        /* The title menu underneath, addressed from the pushed-record base. */
        g_open_controller_records_next[g_open_current_controller_index - 1].title.idle_timer = 0;
        return;
    }

    if (g_open_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_START)) {
        suzuki_sound_set_type(menu->sound_mode_selection);
        g_main_game_options.fields.sound_mode = menu->sound_mode_selection;
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
        menu->step = 2;
        return;
    }
    if (g_open_input_new_button_presses & PSX_PAD_CROSS) {
        main_sound_play_sfx(MAIN_SFX_CANCEL);
        menu->step = 2;
        return;
    }

    if (open_input_update_repeat(PSX_PAD_UP) != 0) {
        render_record = menu->entry_render_indices[menu->sound_mode_selection];
        g_open_gfx_render_records_36[render_record].palette = 0;
        menu->sound_mode_selection -= 1;
        if (menu->sound_mode_selection < GAME_SOUND_MODE_MONO) {
            menu->sound_mode_selection = GAME_SOUND_MODE_WIDE;
        }
        render_record = menu->entry_render_indices[menu->sound_mode_selection];
        g_open_gfx_render_records_36[render_record].palette = OPEN_MENU_ENTRY_HIGHLIGHTED_PALETTE;
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }

    if (open_input_update_repeat(PSX_PAD_DOWN) != 0 || (g_open_input_new_button_presses & PSX_PAD_SELECT) != 0) {
        render_record = menu->entry_render_indices[menu->sound_mode_selection];
        g_open_gfx_render_records_36[render_record].palette = 0;
        menu->sound_mode_selection += 1;
        if (menu->sound_mode_selection > GAME_SOUND_MODE_WIDE) {
            menu->sound_mode_selection = GAME_SOUND_MODE_MONO;
        }
        render_record = menu->entry_render_indices[menu->sound_mode_selection];
        g_open_gfx_render_records_36[render_record].palette = OPEN_MENU_ENTRY_HIGHLIGHTED_PALETTE;
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }
}
