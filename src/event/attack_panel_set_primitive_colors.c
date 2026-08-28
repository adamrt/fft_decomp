#include "fft/attack.h"
#include "fft/battle.h"
#include "psx/types.h"

/* status_panel_primitives_t: a 0x10-byte header followed by 0x14-byte SPRT
 * packets.  The layout follows from this function: the colour writes at
 * +0x1a4..0x1a6 (after SetShadeTex on +0x1a0) only land on a packet's r0/g0/b0
 * if the array starts at +0x10, and the 0x7cxx/0x7dxx values then fall on the
 * clut halfword (+0xe).  Sprites 1..19 get their clut, 20..24 their colour.
 * The BUNIT, DEBUGCHR, and REQUIRE implementations use the same layout.
 *
 * The target's strength-reduced pointers are biased from the block base
 * (+0xdc/+0xf0 with a 0x32 displacement, +0x1a4 for the colour bytes).  The
 * array-subscript form `sprites[i + 1].clut` does not reproduce that: GCC
 * 2.6.3 keeps `(i + 1) * 0x14` as its own counter and folds the 0x10 header
 * into a 0x1e displacement.  Written as pointer arithmetic,
 * `(sprites + i + 1)->clut`, the whole constant part folds to base + 0x32
 * and the walkers come out as the target has them, with no pins.  The
 * SetShadeTex argument is the one place the subscript form is right: it
 * yields the target's separate 0x1a0 integer offset giv. */
void attack_panel_set_primitive_colors(
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
