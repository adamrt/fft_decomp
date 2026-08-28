#include "fft/battle.h"
#include "fft/map.h"
#include "psx/types.h"

void battle_map_init_ambient_light(s32 unused_0, s32 unused_1, s32 map_id) {
    /* Pin: the target copies the record pointer into $a2 (`move a2,v0`);
     * unpinned GCC uses $v0 directly, and reusing the map_id
     * parameter for it does not produce the copy either. */
    register map_color_t* color asm("$6");

    color = (map_color_t*)battle_map_light_state_command(MAP_LIGHT_COMMAND_GET_AMBIENT_COLOR, (u8*)map_id);
    /* Makes the `move a2,v0` copy happen right after the call. */
    asm volatile("" : "=r"(color) : "0"(color));
    g_battle_map_ambient_light_color = *color;
    g_battle_map_ambient_light_transition.channels.red = color->red << 16;
    g_battle_map_ambient_light_transition.channels.green = color->green << 16;
    g_battle_map_ambient_light_transition.channels.blue = color->blue << 16;
}
