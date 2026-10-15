#include "fft/battle_menu.h"
#include "fft/event_card.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/types.h"

/*
 * Thread body of the save screen's zodiac backdrop: draws it every frame
 * until the third thread parameter asks it to stop.
 *
 * It is centerd at (0x100, 0x80), or at (0, 8) on the formation screen.
 * With the first parameter zero it starts at twice its size (4.12 scale
 * 0x2000) and shrinks by 0x100 per event-speed step to 0x1000.
 */
void card_save_run_menu_graphic_thread(void) {
    s32 frame;

    if (g_battle_formation_screen_active != 0) {
        g_card_input_display_state.offset.position.x = 0;
        g_card_input_display_state.offset.position.y = 8;
    } else {
        g_card_input_display_state.offset.position.x = 0x100;
        g_card_input_display_state.offset.position.y = 0x80;
    }

    /* Start at 2.0 (4.12) and shrink to 1.0 below, or start at 1.0. */
    if (thread_get_current_parameter_1_variant() != 0) {
        g_card_input_display_state.scale_x = 0x1000;
        g_card_input_display_state.scale_y = 0x1000;
    } else {
        g_card_input_display_state.scale_x = 0x2000;
        g_card_input_display_state.scale_y = 0x2000;
    }

    for (frame = 0;; frame++) {
        g_card_input_display_state.ot = g_card_gfx_render_otag;
        if (g_card_input_display_state.scale_x > 0x1000) {
            s32 delta = g_script_event_speed << 8;

            g_card_input_display_state.scale_x -= delta;
            g_card_input_display_state.scale_y -= delta;
        }
        g_card_input_display_state.brightness = 0x80;
        menu_build_zodiac_primitives(
            &g_card_input_display_state, g_card_gfx_scaled_sprite_primitive_buffers[frame & 1]);
        thread_yield();
        if (thread_get_current_parameter_3_variant() != 0) {
            thread_exit_current();
        }
    }
}
