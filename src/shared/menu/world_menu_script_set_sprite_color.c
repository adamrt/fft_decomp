#include "fft/world.h"

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
