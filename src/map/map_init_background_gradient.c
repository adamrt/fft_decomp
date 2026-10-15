#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "psx/types.h"

/* Load the two map background gradient colors for the current map. */
void map_init_background_gradient(void) {
    map_background_gradient_colors_t* colors;

    colors = map_light_state_command(MAP_LIGHT_COMMAND_GET_BACKGROUND_GRADIENT, colors);
    g_map_background_gradient_transition.colors[0] = colors->colors[MAP_BACKGROUND_GRADIENT_TOP];
    g_map_background_gradient_transition.colors[1] = colors->colors[MAP_BACKGROUND_GRADIENT_BOTTOM];
    g_map_background_gradient_transition.channels[0].red = colors->colors[MAP_BACKGROUND_GRADIENT_TOP].r << 16;
    g_map_background_gradient_transition.channels[0].green = colors->colors[MAP_BACKGROUND_GRADIENT_TOP].g << 16;
    g_map_background_gradient_transition.channels[0].blue = colors->colors[MAP_BACKGROUND_GRADIENT_TOP].b << 16;
    g_map_background_gradient_transition.channels[1].red = colors->colors[MAP_BACKGROUND_GRADIENT_BOTTOM].r << 16;
    g_map_background_gradient_transition.channels[1].green = colors->colors[MAP_BACKGROUND_GRADIENT_BOTTOM].g << 16;
    g_map_background_gradient_transition.channels[1].blue = colors->colors[MAP_BACKGROUND_GRADIENT_BOTTOM].b << 16;
}
