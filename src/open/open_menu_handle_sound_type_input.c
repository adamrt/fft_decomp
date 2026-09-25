#include "fft/open.h"
#include "psx/pad.h"

#define OPEN_SOUND_MENU_ENTRY_HIGHLIGHTED 9

void open_menu_handle_sound_type_input(open_sound_menu_state_t* menu) {
    s32 record;

    if (menu->step == 2) {
        g_open_gfx_next_render_record_36 -= 3;
        g_open_gfx_render_record_pointer_count -= 3;
        g_open_current_controller_index -= 1;
        open_gfx_hide_four_render_records(
            (const open_birthday_render_record_indices_t*)&g_open_controller_records[g_open_current_controller_index]);
        /* The popped controller's own record, addressed from the pushed-record base. */
        g_open_controller_stream_start[g_open_current_controller_index - 1]._unknown_14 = 0;
        return;
    }

    if (g_open_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_START)) {
        main_sound_set_type(menu->sound_mode_selection);
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

    if (open_input_check_repeating_directional(PSX_PAD_UP) != 0) {
        record = menu->render_records[menu->sound_mode_selection];
        g_open_gfx_render_records_36[record].palette = 0;
        menu->sound_mode_selection -= 1;
        if (menu->sound_mode_selection < GAME_SOUND_MODE_MONO) {
            menu->sound_mode_selection = GAME_SOUND_MODE_WIDE;
        }
        record = menu->render_records[menu->sound_mode_selection];
        g_open_gfx_render_records_36[record].palette = OPEN_SOUND_MENU_ENTRY_HIGHLIGHTED;
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }

    if (open_input_check_repeating_directional(PSX_PAD_DOWN) != 0
        || (g_open_input_new_button_presses & PSX_PAD_SELECT) != 0) {
        record = menu->render_records[menu->sound_mode_selection];
        g_open_gfx_render_records_36[record].palette = 0;
        menu->sound_mode_selection += 1;
        if (menu->sound_mode_selection > GAME_SOUND_MODE_WIDE) {
            menu->sound_mode_selection = GAME_SOUND_MODE_MONO;
        }
        record = menu->render_records[menu->sound_mode_selection];
        g_open_gfx_render_records_36[record].palette = OPEN_SOUND_MENU_ENTRY_HIGHLIGHTED;
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }
}
