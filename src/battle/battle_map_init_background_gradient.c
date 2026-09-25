#include "fft/battle.h"
#include "psx/types.h"

/* Load the two map background gradient colours for the current map. */
void battle_map_init_background_gradient(s32 unused_0, s32 unused_1, s32 map_id) {
    /* Pin: the target copies the record pointer into $a2 (`move a2,v0`), as in
     * battle_map_init_ambient_light and battle_map_init_darkness;
     * unpinned GCC uses $v0 directly. */
    register map_background_gradient_colors_t* colors asm("$6");

    colors = (map_background_gradient_colors_t*)battle_map_light_state_command(
        MAP_LIGHT_COMMAND_GET_BACKGROUND_GRADIENT, (u8*)map_id);
    /* Emits nothing; makes the pin take effect as the `move a2,v0` copy. */
    asm volatile("" : "=r"(colors) : "0"(colors));
    g_map_background_gradient_transition.colors[0] = colors->first;
    g_map_background_gradient_transition.colors[1] = colors->second;
    g_map_background_gradient_transition.channels[0].red = colors->first.red << 16;
    g_map_background_gradient_transition.channels[0].green = colors->first.green << 16;
    g_map_background_gradient_transition.channels[0].blue = colors->first.blue << 16;
    g_map_background_gradient_transition.channels[1].red = colors->second.red << 16;
    g_map_background_gradient_transition.channels[1].green = colors->second.green << 16;
    g_map_background_gradient_transition.channels[1].blue = colors->second.blue << 16;
}
