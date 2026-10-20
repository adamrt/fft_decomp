/* The render ordering table and the save menu graphic thread. */

#include "fft/formation.h"
#include "fft/save.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/zodiac.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

extern zodiac_draw_context_t g_save_input_display_state;

/* Double-buffered ZODIAC sprite chains (0x4b0 bytes each). */
static POLY_FT4 g_save_gfx_scaled_sprite_primitive_buffers[2][30] = { 0 };
static u32* g_save_gfx_render_otag = 0;

/* Set the ordering-table entry the card screen's graphic thread draws into. */
void save_gfx_set_render_otag(u32* otag) {
    g_save_gfx_render_otag = otag;
}

/*
 * Thread body of the save screen's zodiac backdrop: draws it every frame
 * until the third thread parameter asks it to stop.
 *
 * It is centered at (0x100, 0x80). The (0, 8) branch for
 * g_formation_screen_active never runs: nothing in BATTLE, which CARD.OUT
 * links against, writes its copy of that flag.
 * With the first parameter zero it starts at twice its size and shrinks by
 * 1/16 (0x100) per event-speed step to full size.
 */
void save_backdrop_thread(void) {
    s32 frame;

    if (g_formation_screen_active != 0) {
        g_save_input_display_state.offset.position.x = 0;
        g_save_input_display_state.offset.position.y = 8;
    } else {
        g_save_input_display_state.offset.position.x = 0x100;
        g_save_input_display_state.offset.position.y = 0x80;
    }

    if (thread_current_parameter_1_get() != 0) {
        g_save_input_display_state.scale_x = ONE;
        g_save_input_display_state.scale_y = ONE;
    } else {
        g_save_input_display_state.scale_x = ONE * 2;
        g_save_input_display_state.scale_y = ONE * 2;
    }

    for (frame = 0;; frame++) {
        g_save_input_display_state.ot = g_save_gfx_render_otag;
        if (g_save_input_display_state.scale_x > ONE) {
            s32 delta = g_script_event_speed << 8;

            g_save_input_display_state.scale_x -= delta;
            g_save_input_display_state.scale_y -= delta;
        }
        g_save_input_display_state.brightness = 0x80;
        zodiac_build_primitives(&g_save_input_display_state, g_save_gfx_scaled_sprite_primitive_buffers[frame & 1]);
        thread_yield();
        if (thread_current_parameter_3_get() != 0) {
            thread_current_exit();
        }
    }
}
