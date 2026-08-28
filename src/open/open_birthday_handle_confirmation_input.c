#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/main_unit.h"
#include "fft/open.h"
#include "fft/script_variables.h"
#include "fft/world.h"
#include "psx/pad.h"

/* Handle confirmation, cancellation, and cursor movement on the final birthday prompt.
 *
 * Confirming the selected date stores Ramza's birthday and zodiac data before starting the opening transition.
 */
void open_birthday_handle_confirmation_input(open_birthday_confirmation_state_t* menu) {
    s32 month;
    s32 day;
    s32 value;
    party_data_t* member;
    u32 buttons;

    if ((g_open_system_runtime_flags & OPEN_RUNTIME_INPUT_DISABLED)
        && !(g_open_gfx_overlay_fade.flags & OPEN_OVERLAY_FADE_ACTIVE)) {
        g_open_system_runtime_flags ^= OPEN_RUNTIME_INPUT_DISABLED;
        main_sound_unload_current_scenario_music();
        g_active_graphics_buffer_index = 1;
        g_open_gfx_next_render_record_56 -= 3;
        g_open_gfx_next_render_record_36 -= 3;
        g_open_gfx_render_record_pointer_count -= 6;
        g_open_current_controller_index -= 2;
        open_movie_stream_and_push_controller_1(0x17B60, 0x64D8, 0x9E4);
        return;
    }

    buttons = g_open_input_new_button_presses;
    if (buttons & PSX_PAD_CIRCLE) {
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
        if (menu->selected_option == 0) {
            month = g_open_controller_birthdays[g_open_current_controller_index - 2].month;
            day = g_open_controller_birthdays[g_open_current_controller_index - 2].day;
            world_script_set_variable(EVENT_SCRIPT_VAR_PLAYER_BIRTH_MONTH, month);
            world_script_set_variable(EVENT_SCRIPT_VAR_PLAYER_BIRTH_DAY, day);
            value = open_birthday_calculate_day_index(month, day);
            open_birthday_convert_to_zodiac_position(&month, &day);
            member = main_party_get_data_pointer(0);
            member->birthday_day = value;
            member->zodiac = ((value & 0x100) >> 8) | ((month - 1) * 0x10);
            main_sound_set_current_music_target(0, 0x60);
            open_gfx_start_overlay_fade_in(0x20);
            g_open_system_runtime_flags |= OPEN_RUNTIME_INPUT_DISABLED;
            return;
        }
        g_open_gfx_render_record_pointer_count -= 3;
        g_open_gfx_next_render_record_56 -= 1;
        g_open_gfx_next_render_record_36 -= 2;
        g_open_current_controller_index -= 1;
        open_restore_birthday_date_menu(
            (open_birthday_date_state_t*)&g_open_controller_records[g_open_current_controller_index]);
        return;
    }
    if (buttons & PSX_PAD_CROSS) {
        main_sound_play_sfx(MAIN_SFX_CANCEL);
        /* Same cancel path as above; GCC cross-jumps the two copies into this one. */
        g_open_gfx_render_record_pointer_count -= 3;
        g_open_gfx_next_render_record_56 -= 1;
        g_open_gfx_next_render_record_36 -= 2;
        g_open_current_controller_index -= 1;
        open_restore_birthday_date_menu(
            (open_birthday_date_state_t*)&g_open_controller_records[g_open_current_controller_index]);
        return;
    }

    if (open_input_check_repeating_directional(PSX_PAD_UP) != 0
        || open_input_check_repeating_directional(PSX_PAD_DOWN) != 0) {
        menu->selected_option ^= 1;
        /* Adding 12 before the option offset keeps the target's (option * 16 + 12) + y evaluation. */
        g_open_gfx_render_records_36[menu->cursor_record_36].y
            = g_open_gfx_render_records_56[menu->menu_record_56].y + 12 + menu->selected_option * 16;
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }
}
