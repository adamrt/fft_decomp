#include "fft/main_sound.h"
#include "fft/open.h"
#include "psx/pad.h"

/* The title-exit controller's view of its open_controller_record_t. */
typedef struct open_title_exit_state {
    s32 state;
} open_title_exit_state_t;

void open_title_update_exit_transition(open_title_exit_state_t* transition) {
    s32 state;
    u32 buttons;

    if ((g_open_gfx_overlay_fade.flags & 4) != 0) {
        return;
    }

    if (transition->state != 0) {
        g_open_gfx_next_render_record_36 -= 1;
        g_open_gfx_render_record_pointer_count -= 1;
        g_open_current_controller_index -= 1;
        state = transition->state;
        if (state == 1) {
            s32* source;
            s32* destination;
            s32 index;

            SetDispMask(0);
            index = 0;
            source = (s32*)g_open_work_buffer_0;
            g_open_system_runtime_flags |= 0x40;
            destination = (s32*)g_open_file_destination;
            do {
                *destination = *source;
                source++;
                index++;
                destination++;
            } while (index < 0x7800);
            open_title_push_menu_controller(0);
        } else if (state == 2) {
            open_title_start_new_game_transition();
        }
        return;
    }

    buttons = g_open_input_new_button_presses;
    if ((buttons & PSX_PAD_CROSS) != 0) {
        main_sound_play_sfx(MAIN_SFX_CANCEL);
        open_gfx_start_overlay_fade_in(0x20);
        transition->state = 1;
    } else if ((buttons & (PSX_PAD_CIRCLE | PSX_PAD_START)) != 0) {
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
        open_gfx_start_overlay_fade_in(0x20);
        transition->state = 2;
    }
}
