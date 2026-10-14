#include "fft/open.h"

/* Opcode 1: create and position a text record from an ending-script command
 * (opcode, x, y, text offset). Eight bytes.
 *
 * A coordinate of -1 centers the record on its last glyph's extent. X bit 0x100
 * instead centers the glyph selected by its low byte. */
void open_opcode_create_positioned_text_record(s16* command) {
    s32 x;
    s32 y;
    s32 index;

    command++;
    x = *command++;
    y = *command++;
    index = open_script_build_text_record(
        open_script_add_pointer_offset(g_open_script_state.dispatch.cursor.data_base, *command));
    if (index != 0) {
        index--;
        if (x == -1) {
            s32 last = g_open_script_state.records[index].glyph_count - 1;

            x = 256
                - (g_open_script_state.records[index].glyphs[last].x
                      + g_open_script_state.records[index].glyphs[last].w)
                    / 2;
        }
        if (y == -1) {
            s32 last = g_open_script_state.records[index].glyph_count - 1;

            y = 120
                - (g_open_script_state.records[index].glyphs[last].y
                      + g_open_script_state.records[index].glyphs[last].h)
                    / 2;
        }
        if (x & 0x100) {
            s32 glyph = x & 0xFF;

            x = 0x100 - g_open_script_state.records[index].glyphs[glyph].x
                - g_open_script_state.records[index].glyphs[glyph].w / 2;
        }
        g_open_script_state.records[index].x = x;
        g_open_script_state.records[index].y = y;
    }
    g_open_script_state.dispatch.cursor.byte_offset += 8;
}
