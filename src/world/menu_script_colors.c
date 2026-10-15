/* Blend draw mode and sprite color menu-script opcodes. */

#include "fft/world.h"
#include "psx/libgpu.h"

menu_instruction_t* world_menu_script_add_blend_draw_mode(menu_instruction_t* instruction) {
    menu_gfx_enqueue_draw_mode_variant(
        0, 0, GetTPage(0, instruction->blend.blend_rate, 0x100, 0), 0, g_menu_gfx_otag_index);
    return menu_next_instruction(instruction);
}

/* Set the current sprite color from a menu-script operand triplet.
 *
 * The palette selector chooses bytes 5..7 instead of the normal bytes 2..4. */
menu_instruction_t* world_menu_script_set_sprite_color(menu_instruction_t* instruction) {
    if (g_world_menu_use_alternate_palette != 0) {
        g_world_menu_sprite_color.r = instruction->sprite_color.alternate.r,
        g_world_menu_sprite_color.g = instruction->sprite_color.alternate.g,
        g_world_menu_sprite_color.b = instruction->sprite_color.alternate.b;
    } else {
        g_world_menu_sprite_color.r = instruction->sprite_color.normal.r,
        g_world_menu_sprite_color.g = instruction->sprite_color.normal.g,
        g_world_menu_sprite_color.b = instruction->sprite_color.normal.b;
    }
    return menu_next_instruction(instruction);
}
