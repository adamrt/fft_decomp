#include "fft/battle.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Draw a decimal value right-to-left from the menu text origin.
 *
 * The origin is first advanced to the last of `digit_count` cells and then
 * walked back one 6-pixel cell per digit, so a shorter value leaves the leading
 * cells untouched rather than zero-padding them. Digits index the same 35-byte
 * glyph records the menu text renderers use.
 *
 * The cell step is read into an `s32` instead of the shorter
 * `*origin -= 6`, which costs nothing here and is what hoists the div-by-10
 * magic constant into $s2 ahead of the loop (and so saves $s3 and uses a 0x28
 * frame, as the target does). Written on the halfword lvalue, the step is
 * `+ 65530` in HImode and needs its own constant insn, which makes the loop 27
 * RTL insns; `loop.c:move_movables` then finds
 * threshold * savings * lifetime < insn_count for the magic constant and leaves
 * it in the loop body. The `s32` temporary folds -6 into the add at expand, the
 * loop is 26 insns, and the constant is hoisted. */
void battle_display_menu_number_entry(s32 value, u8 digit_count, s32 image, u16* origin) {
    s32 cell;

    *origin += (digit_count - 1) * 6;
    do {
        battle_text_render_glyph_to_4bpp_image(
            g_text_glyph_bitmap_data + (value % 10) * 35, image, origin, g_menu_text_palette_offset);
        value = value / 10;
        cell = *origin;
        *origin = cell - 6;
    } while (value != 0);
    g_menu_text_palette_offset = 0;
}
