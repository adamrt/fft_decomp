#include "fft/open.h"

/* Create and position a text record from an opening-script command.
 *
 * A coordinate of -1 centers the final glyph bounds. X bit 0x100 instead
 * centers the glyph selected by its low byte. The byte-offset aliases preserve
 * target register allocation where direct record indexing changes codegen.
 */
void open_opcode_create_positioned_text_record(const s16* command) {
    s32 x;
    s32 y;
    s32 index;
    s32 last;
    const s16* operand;

    operand = command + 1;
    /* Keeps p a stepped pointer; otherwise the reads fold to constant offsets from command. */
    __asm__("" : "=r"(operand) : "0"(operand));
    x = *operand++;
    y = *operand;
    index = open_script_build_text_record(
        (u8*)open_script_add_pointer_offset(g_open_script_state.dispatch.data_base, operand[1]));
    if (index != 0) {
        index -= 1;
        if (x == -1) {
            /* Pins required (all three blocks): unpinned, the -1 sentinel and the offsets rotate among a0/a2/v0/v1 and
             * an extra move appears. */
            register s32 record_offset __asm__("$3");
            register s32 glyph_offset __asm__("$2");

            record_offset = index * sizeof(open_script_record_t);
            last = *(s32*)(g_open_script_record_glyph_count_field_base + record_offset) - 1;
            glyph_offset = last * sizeof(open_script_glyph_t) + record_offset;
            x = 256
                - (*(s16*)(g_open_script_glyph_x_field_base + glyph_offset)
                      + *(s16*)(g_open_script_glyph_width_field_base + glyph_offset))
                    / 2;
        }
        if (y == -1) {
            register s32 record_offset __asm__("$3");
            register s32 glyph_offset __asm__("$2");

            record_offset = index * sizeof(open_script_record_t);
            last = *(s32*)(g_open_script_record_glyph_count_field_base + record_offset) - 1;
            glyph_offset = last * sizeof(open_script_glyph_t) + record_offset;
            y = 120
                - (*(s16*)(g_open_script_glyph_y_field_base + glyph_offset)
                      + *(s16*)(g_open_script_glyph_height_field_base + glyph_offset))
                    / 2;
        }
        if (x & 0x100) {
            register s32 glyph_offset __asm__("$4");
            register s32 width __asm__("$2");
            s32 record_offset;

            glyph_offset = (x & 0xFF) * sizeof(open_script_glyph_t);
            record_offset = index * sizeof(open_script_record_t);
            glyph_offset += record_offset;
            width = *(u16*)(g_open_script_glyph_width_field_base + glyph_offset);
            {
                s32 glyph_x;
                s32 centered_x;
                s32 half_width;

                glyph_x = *(s16*)(g_open_script_glyph_x_field_base + glyph_offset);
                half_width = (s16)width / 2;
                centered_x = 0x100 - half_width;
                x = centered_x - glyph_x;
            }
        }
        g_open_script_state.records[index].x = x;
        g_open_script_state.records[index].y = y;
    }
    {
        s32* script_byte_offset;

        script_byte_offset = &g_open_script_state.dispatch.byte_offset;
        *script_byte_offset += 8;
    }
}
