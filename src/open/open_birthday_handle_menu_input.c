#include "fft/open.h"
#include "psx/pad.h"

#define OPEN_BIRTHDAY_MONTH_COUNT 12
#define OPEN_BIRTHDAY_CURSOR_STEP 0x20
#define OPEN_PAD_START_OR_CIRCLE  0x820

/* Per-frame handler of the birthday date-entry controller (handler 5).
 *
 * Input is ignored while the date window is popping in. Start or Circle recolors
 * the menu for the prompt and pushes the confirmation controller. Up and Down
 * (with auto-repeat) step the selected field: a day rolls over into the next or
 * previous month, a month wraps between December and January and clamps the day
 * to the new month's length. Each change releases the date window record (the
 * last one allocated) and rebuilds it. Left selects the month field and Right the
 * day field, moving the cursor 0x20 pixels.
 *
 * February has 28 days in g_open_birthday_month_lengths, so 29 February cannot be
 * entered. */
void open_birthday_handle_menu_input(open_controller_record_t* record) {
    open_birthday_date_state_t* menu = &record->birthday_date;
    if (g_open_gfx_render_records_56[menu->window_record_56].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }

    if (g_open_input_new_button_presses & OPEN_PAD_START_OR_CIRCLE) {
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
        open_birthday_build_confirmation_menu(menu);
        open_birthday_push_confirmation_controller();
        return;
    }

    if (open_input_update_repeat(PSX_PAD_UP) != 0) {
        if (menu->selecting_month == 0) {
            menu->day += 1;
            if (g_unit_birthday_month_lengths[menu->month - 1] < menu->day) {
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
            if (g_unit_birthday_month_lengths[menu->month - 1] < menu->day) {
                menu->day = g_unit_birthday_month_lengths[menu->month - 1];
            }
        }
        g_open_gfx_render_record_pointer_count -= 1;
        g_open_gfx_next_render_record_56 -= 1;
        open_birthday_build_menu_text(menu);
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    } else if (open_input_update_repeat(PSX_PAD_DOWN) != 0) {
        if (menu->selecting_month == 0) {
            menu->day -= 1;
            if (menu->day <= 0) {
                menu->month -= 1;
                if (menu->month <= 0) {
                    menu->month = OPEN_BIRTHDAY_MONTH_COUNT;
                }
                menu->day = g_unit_birthday_month_lengths[menu->month - 1];
            }
        } else {
            menu->month -= 1;
            if (menu->month <= 0) {
                menu->month = OPEN_BIRTHDAY_MONTH_COUNT;
            }
            if (g_unit_birthday_month_lengths[menu->month - 1] < menu->day) {
                menu->day = g_unit_birthday_month_lengths[menu->month - 1];
            }
        }
        g_open_gfx_render_record_pointer_count -= 1;
        g_open_gfx_next_render_record_56 -= 1;
        open_birthday_build_menu_text(menu);
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }

    if ((g_open_input_new_button_presses & PSX_PAD_LEFT) && menu->selecting_month == 0) {
        menu->selecting_month = 1;
        g_open_gfx_render_records_36[menu->cursor_record_36].position.x -= OPEN_BIRTHDAY_CURSOR_STEP;
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }

    if ((g_open_input_new_button_presses & PSX_PAD_RIGHT) && menu->selecting_month != 0) {
        menu->selecting_month = 0;
        g_open_gfx_render_records_36[menu->cursor_record_36].position.x += OPEN_BIRTHDAY_CURSOR_STEP;
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }
}
