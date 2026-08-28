#include "fft/world.h"
#include "psx/gte.h"
#include "psx/types.h"

/* Set the current sprite color from a menu-script operand triplet.
 *
 * The palette selector chooses bytes 5..7 instead of the normal bytes 2..4. */
u8* world_menu_script_set_sprite_color(u8* script) {
    if (g_world_menu_use_alternate_palette != 0) {
        g_world_menu_sprite_color.r = script[5], g_world_menu_sprite_color.g = script[6],
        g_world_menu_sprite_color.b = script[7];
    } else {
        g_world_menu_sprite_color.r = script[2], g_world_menu_sprite_color.g = script[3],
        g_world_menu_sprite_color.b = script[4];
    }
    return script + script[1];
}
