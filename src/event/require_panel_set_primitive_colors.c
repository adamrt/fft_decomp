#include "fft/battle.h"
#include "fft/require.h"
#include "psx/types.h"

/* status_panel_primitives_t: a 0x10-byte header followed by 0x14-byte SPRT
 * packets.  The layout follows from this function: the colour writes at
 * +0x1a4..0x1a6 (after SetShadeTex on +0x1a0) only land on a packet's r0/g0/b0
 * if the array starts at +0x10, and the 0x7cxx/0x7dxx values then fall on the
 * clut halfword (+0xe).  Sprites 1..19 get their clut, 20..24 their colour.
 * The ATTACK, BUNIT, and DEBUGCHR implementations use the same layout.
 *
 * The walkers use the pointer-arithmetic form `(sprites + i + 1)->clut` so
 * the whole constant part folds to base + 0x32 and strength-reduces to the
 * target's base-biased pointers (+0xdc/+0xf0/+0x1a4), with no pins; the
 * subscript form `sprites[i + 1].clut` keeps `(i + 1) * 0x14` as its own
 * counter.  See attack_panel_set_primitive_colors.c. */
void require_panel_set_primitive_colors(
    status_panel_primitives_t* primitives, const status_panel_frame_config_t* state) {
    s32 i;
    s32 clut;

    if (state->style == 1 || g_event_mode == 1) {
        clut = 0x7dfc;
        for (i = 11; i >= 0; i--) {
            (primitives->sprites + i + 1)->clut = clut;
        }
        for (i = 12; i < 19; i++) {
            (primitives->sprites + i + 1)->clut = 0x7d3c;
        }
        primitives->sprites[1].clut = 0x7c7c;
        primitives->sprites[2].clut = 0x7c7c;
        for (i = 0; i < 5; i++) {
            SetShadeTex(&primitives->sprites[i + 20], 0);
            (primitives->sprites + i + 20)->r0 = 0x40;
            (primitives->sprites + i + 20)->g0 = 0x40;
            (primitives->sprites + i + 20)->b0 = 0x80;
        }
    } else {
        clut = 0x7d7c;
        for (i = 11; i >= 0; i--) {
            (primitives->sprites + i + 1)->clut = clut;
        }
        for (i = 12; i < 19; i++) {
            (primitives->sprites + i + 1)->clut = 0x7c3c;
        }
        primitives->sprites[1].clut = 0x7cbc;
        primitives->sprites[2].clut = 0x7cbc;
        for (i = 0; i < 5; i++) {
            SetShadeTex(&primitives->sprites[i + 20], 0);
            (primitives->sprites + i + 20)->r0 = 0x80;
            (primitives->sprites + i + 20)->g0 = 0x80;
            (primitives->sprites + i + 20)->b0 = 0x80;
        }
    }
}
