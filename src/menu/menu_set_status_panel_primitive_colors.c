#include "fft/battle_event.h"
#include "fft/menu.h"

/* The full panel's SPRTs begin at +0x24. The CLUT walks use pointer
 * arithmetic to keep the target's base-biased pointers; SetShadeTex uses a
 * subscript to retain its separate integer offset. */
void menu_set_status_panel_primitive_colors(menu_status_panel_buffer_t* primitives, const menu_status_thread_t* state) {
    s32 i;
    s32 clut;

    if (state->style == 1 || g_menu_input_lock_mode == 1) {
        clut = 0x7dfc;
        for (i = 11; i >= 0; i--) {
            (primitives->sprites + i)->clut = clut;
        }
        for (i = 12; i < 19; i++) {
            (primitives->sprites + i)->clut = 0x7d3c;
        }
        primitives->sprites[0].clut = 0x7c7c;
        primitives->sprites[1].clut = 0x7c7c;
        for (i = 0; i < 5; i++) {
            SetShadeTex(&primitives->sprites[i + 19], 0);
            (primitives->sprites + i + 19)->r0 = 0x40;
            (primitives->sprites + i + 19)->g0 = 0x40;
            (primitives->sprites + i + 19)->b0 = 0x80;
        }
    } else {
        clut = 0x7d7c;
        for (i = 11; i >= 0; i--) {
            (primitives->sprites + i)->clut = clut;
        }
        for (i = 12; i < 19; i++) {
            (primitives->sprites + i)->clut = 0x7c3c;
        }
        primitives->sprites[0].clut = 0x7cbc;
        primitives->sprites[1].clut = 0x7cbc;
        for (i = 0; i < 5; i++) {
            SetShadeTex(&primitives->sprites[i + 19], 0);
            (primitives->sprites + i + 19)->r0 = 0x80;
            (primitives->sprites + i + 19)->g0 = 0x80;
            (primitives->sprites + i + 19)->b0 = 0x80;
        }
    }
}
