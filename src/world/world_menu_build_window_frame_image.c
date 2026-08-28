#include "fft/world.h"

/* Build a 4bpp window-frame image from the shared menu graphics.
 *
 * Top and bottom bands are fixed; the middle 16 rows tile. The unused `s32`
 * return keeps $v0 live and preserves the target loop scheduling.
 */
s32 world_menu_build_window_frame_image(s32 w, s32 h, u16* dst) {
    u16* src = (u16*)g_world_menu_glyph_sheet;
    s32 words;
    s32 i;
    s32 first = 0;
    u16* base = dst;
    s32 src_index;
    u16* row;
    s32 has_partial_word;

    words = w & 3;
    has_partial_word = words != 0;
    words = (w >> 2) + has_partial_word;
    for (i = 0; i < h; i++) {
        src_index = i << 6;
        if (i >= 8) {
            if (i >= h - 8) {
                src_index = (((i - h + 8) & 7) << 6) + 0x600;
            } else {
                src_index = (((i - 8) & 0xF) + 8) << 6;
            }
        }
        row = base + (i - first) * words;
        row[words - 1] = src[src_index + 9];
        row[words - 2] = src[src_index + 8];
        world_menu_fill_pattern_row_2(src, row, words, src_index);
    }
}
