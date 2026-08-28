#include "fft/main_sound.h"
#include "fft/open.h"
#include "psx/pad.h"

#define OPEN_BIRTHDAY_MONTH_COUNT 12
#define OPEN_BIRTHDAY_CURSOR_STEP 0x20
#define OPEN_PAD_START_OR_CIRCLE  0x820
/* g_open_birthday_month_lengths indexed by the 1-based month. A local pointer copy
 * gets a callee-saved register and grows the frame; keep the inline form. */
#define OPEN_MONTH_LENGTH(month) ((g_open_birthday_month_lengths - 1)[month])

void open_birthday_handle_menu_input(open_birthday_date_state_t* menu) {
    if (g_open_gfx_render_records_56[menu->window_record_56].flags & 0x100) {
        return;
    }

    if (g_open_input_new_button_presses & OPEN_PAD_START_OR_CIRCLE) {
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
        open_birthday_build_confirmation_menu(menu);
        open_birthday_push_confirmation_controller();
        return;
    }

    if (open_input_check_repeating_directional(PSX_PAD_UP) != 0) {
        if (menu->selecting_month == 0) {
            menu->day += 1;
            if (OPEN_MONTH_LENGTH(menu->month) < menu->day) {
                menu->month += 1;
                if (menu->month > OPEN_BIRTHDAY_MONTH_COUNT) {
                    menu->month = 1;
                }
                menu->day = 1;
            }
        } else {
            menu->month += 1;
            if (menu->month > OPEN_BIRTHDAY_MONTH_COUNT) {
                menu->month = 1;
            }
            if (OPEN_MONTH_LENGTH(menu->month) < menu->day) {
                menu->day = OPEN_MONTH_LENGTH(menu->month);
            }
        }
        g_open_gfx_render_record_pointer_count -= 1;
        g_open_gfx_next_render_record_56 -= 1;
        open_birthday_build_menu_text(menu);
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    } else if (open_input_check_repeating_directional(PSX_PAD_DOWN) != 0) {
        if (menu->selecting_month == 0) {
            menu->day -= 1;
            if (menu->day <= 0) {
                menu->month -= 1;
                if (menu->month <= 0) {
                    menu->month = OPEN_BIRTHDAY_MONTH_COUNT;
                }
                menu->day = OPEN_MONTH_LENGTH(menu->month);
            }
        } else {
            menu->month -= 1;
            if (menu->month <= 0) {
                menu->month = OPEN_BIRTHDAY_MONTH_COUNT;
            }
            if (OPEN_MONTH_LENGTH(menu->month) < menu->day) {
                menu->day = OPEN_MONTH_LENGTH(menu->month);
            }
        }
        g_open_gfx_render_record_pointer_count -= 1;
        g_open_gfx_next_render_record_56 -= 1;
        open_birthday_build_menu_text(menu);
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }

    if ((g_open_input_new_button_presses & PSX_PAD_LEFT) && menu->selecting_month == 0) {
        menu->selecting_month = 1;
        g_open_gfx_render_records_36[menu->cursor_record_36].x -= OPEN_BIRTHDAY_CURSOR_STEP;
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }

    if ((g_open_input_new_button_presses & PSX_PAD_RIGHT) && menu->selecting_month != 0) {
        menu->selecting_month = 0;
        g_open_gfx_render_records_36[menu->cursor_record_36].x += OPEN_BIRTHDAY_CURSOR_STEP;
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }
}
