#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * Render a plain right-aligned number with the menu's compact glyphs.
 *
 * The printed value and the digit count are one variable, which keeps the
 * value in $s2 across the if/else join. The count is accumulated
 * (`value = width; value -= blanks;`) so the re-read width lands in the
 * destination register as the minuend; written as a single subtraction the
 * sign-extended blanks claims $s2 and the operands swap.
 */
menu_instruction_t* world_menu_script_draw_plain_number(menu_instruction_t* instruction) {
    s8 text[16];
    RECT rect;
    s32 value;
    s16 blanks;
    s32 x;
    s32 y;
    s32 i;

    value = g_world_menu_script_callbacks[instruction->number.getter].value != 0
        ? g_world_menu_script_callbacks[instruction->number.getter].value(instruction->number.value)
        : instruction->number.value;
    blanks = instruction->number.width - sprintf(text, "%d", value);
    value = instruction->number.width;
    value -= blanks;
    x = instruction->number.x;
    y = instruction->number.y;
    g_world_menu_number_blank_sprite.position.y = y + 4;
    blanks -= 1;
    while (blanks != -1) {
        g_world_menu_number_blank_sprite.position.x = x;
        menu_gfx_enqueue_oriented_textured_quad(
            &g_world_menu_number_blank_sprite, 0, TEXTURE_UV_FLIP_NONE, 0, g_world_menu_draw_priority);
        blanks -= 1;
        x += 8;
    }
    rect.x = x;
    rect.y = y + 2;
    rect.w = 8;
    rect.h = 14;
    i = 0;
    while (i < value) {
        menu_gfx_enqueue_textured_quad(
            &rect, text[i] * 8 - 0xD8, 0x32, 0, 0, g_world_menu_icon_tpage, 0x7EE7, g_world_menu_draw_priority);
        i++;
        rect.x += 8;
    }
    return menu_next_instruction(instruction);
}
