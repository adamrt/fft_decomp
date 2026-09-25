#include "fft/event_card.h"
#include "psx/types.h"

u8* card_cmd_draw_decimal_number_handler(u8* command) {
    s8 digits[16];
    RECT destination;
    RECT glyph;
    volatile u8 unused_stack[16];
    const u8* color;
    u16 clut;
    s32 destination_y;
    s32 digit_index;
    s32 leading_zeroes;
    s32 column;
    s32 digit;
    char* digit_destination = (char*)digits;
    const char* decimal_format = g_card_text_decimal_format;
    s32 value;
    /* The pin drops the `andi 0xff` GCC otherwise re-applies to the volatile lbu. */
    register s32 column_count __asm__("$2");

    /* Keeps the s3 save and digit_index zeroing after the sprintf argument setup. */
    __asm__("" : : "r"(digit_destination), "r"(decimal_format));
    digit_index = 0;
    /* Keeps digit_index's `move s3,zero` ahead of the ra save. */
    __asm__ volatile("" : : "r"(digit_index) : "memory");
    value = command[9];
    destination_y = command[8];
    clut = g_card_text_digit_clut;
    /* Keeps destination_y and clut in s5/s4 rather than s6/s5. */
    __asm__("" : : "r"(value), "r"(destination_y), "r"(clut));
    glyph.x = 0x78;
    glyph.y = 0x10;
    glyph.w = 6;
    glyph.h = 0xb;
    leading_zeroes = command[4] - sprintf(digit_destination, decimal_format, value);
    /* Keeps digit_index in s3 and leading_zeroes in s2 (unfenced, they swap). */
    __asm__("" : : "r"(leading_zeroes) : "memory");
    column_count = *(volatile u8*)&command[4];
    color = g_card_text_color;

    /* Keeps column's `move s1,zero` out of the lbu delay slot above. */
    __asm__ volatile("" : : "r"(color) : "memory");
    column = 0;
    if (column_count > 0) {
        do {
            destination.x = command[7] + column * command[5];
            destination.y = destination_y;
            destination.w = glyph.w;
            destination.h = glyph.h;
            if ((s16)leading_zeroes != 0) {
                digit = 0;
                leading_zeroes--;
            } else {
                digit = digits[digit_index] - '0';
                digit_index++;
            }
            if (command[2] < 2) {
                card_gfx_enqueue_textured_quad(&destination, glyph.x + glyph.w * digit, glyph.y, color,
                    g_card_gfx_draw_semitrans, g_card_text_digit_texture_page, clut, g_card_text_otag_index);
            }
            column++;
        } while (column < command[4]);
    }
    return command + command[1];
}
