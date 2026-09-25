#include "fft/card.h"
#include "fft/event.h"
#include "psx/types.h"

typedef struct card_input_display_state {
    void* render_otag;
    u32 color; /* 0x04: copied into the poly r/g/b */
    u8 unknown_08[4];
    s16 scale_x; /* 0x0c; 4.12 sprite scale */
    u16 scale_y; /* 0x0e; 4.12 sprite scale */
    u8 unknown_10[0x10];
    u16 offset_x; /* 0x20: offset[4] of the +0x18 view, added to every quad x */
    u16 offset_y; /* 0x22: offset[5], added to every quad y */
} card_input_display_state_t;

extern card_input_display_state_t g_card_input_display_state;

void card_save_run_menu_graphic_thread(void) {
    card_input_display_state_t* state;
    s32 frame;
    s32 scale;
    s32 mode;

    if (g_battle_formation_screen_active != 0) {
        g_card_input_display_state.offset_x = 0;
        mode = 8;
    } else {
        g_card_input_display_state.offset_x = 0x100;
        mode = 0x80;
    }
    g_card_input_display_state.offset_y = mode;

    /* Start at 1.0 or 2.0 (4.12) and shrink to 1.0 below. */
    scale = battle_thread_get_current_parameter_1();
    if (scale != 0) {
        scale = 0x1000;
    } else {
        scale = 0x2000;
    }
    g_card_input_display_scale_x = scale;
    g_card_input_display_scale_y = scale;
    /* Fence: keeps the two scale stores ahead of `frame = 0` /
     * `state = &g_card_input_display_state`; the scheduler otherwise interleaves them. */
    __asm__("" : : : "memory");

    frame = 0;
    state = &g_card_input_display_state;
    for (;;) {
        state->render_otag = g_card_gfx_render_otag;
        if (state->scale_x > 0x1000) {
            s32 delta = g_battle_event_speed << 8;

            state->scale_x -= delta;
            state->scale_y -= delta;
        }
        state->color = 0x80;
        /* The chain builder reads this record through the shared scaled-sprite
         * view, which loads the colour word at +0x04 as a byte intensity and
         * scale_y as s16. */
        card_gfx_build_scaled_sprite_primitive_chain(
            (battle_menu_status_panel_scaled_sprite_t*)state, g_card_gfx_scaled_sprite_primitive_buffers[frame & 1]);
        battle_thread_yield();
        if (battle_thread_get_current_parameter_3() != 0) {
            battle_thread_exit_current();
        }
        frame++;
    }
}
