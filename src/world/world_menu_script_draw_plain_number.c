#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/libc.h"
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
world_draw_number_command_t* world_menu_script_draw_plain_number(world_draw_number_command_t* command) {
    s8 text[16];
    RECT rect;
    s32 value;
    s16 blanks;
    s32 x;
    s32 y;
    s32 i;

    value = g_world_menu_script_callbacks[command->getter] != 0
        ? g_world_menu_script_callbacks[command->getter](command->value)
        : command->value;
    blanks = command->width - sprintf(text, "%d", value);
    value = command->width;
    value -= blanks;
    x = command->x;
    y = command->y;
    g_world_menu_number_blank_sprite.y = y + 4;
    blanks -= 1;
    while (blanks != -1) {
        g_world_menu_number_blank_sprite.x = x;
        world_gfx_enqueue_oriented_textured_quad(
            (const world_oriented_quad_t*)&g_world_menu_number_blank_sprite, 0, 0, 0, g_world_menu_draw_priority);
        blanks -= 1;
        x += 8;
    }
    rect.x = x;
    rect.y = y + 2;
    rect.w = 8;
    rect.h = 14;
    i = 0;
    while (i < value) {
        world_gfx_enqueue_textured_quad(
            &rect, text[i] * 8 - 0xD8, 0x32, 0, 0, g_world_menu_icon_tpage, 0x7EE7, g_world_menu_draw_priority);
        i++;
        rect.x += 8;
    }
    return (world_draw_number_command_t*)((u8*)command + command->length);
}
