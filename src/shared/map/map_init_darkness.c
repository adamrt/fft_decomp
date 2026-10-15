#include "fft/battle_map.h"

/* Seed the darkness fade from the map's current darkness color: the base color
 * and the 16.16 transition channels.
 *
 * The unset `color` is passed as the command's data argument, which the GET
 * command does not read, and then receives the result. */
void map_init_darkness(void) {
    CVECTOR* color;

    color = map_light_state_command(MAP_LIGHT_COMMAND_GET_DARKNESS_COLOR, color);
    g_map_darkness_color = *color;
    g_map_darkness_transition.channels.red = color->r << 16;
    g_map_darkness_transition.channels.green = color->g << 16;
    g_map_darkness_transition.channels.blue = color->b << 16;
}
