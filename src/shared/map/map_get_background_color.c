#include "fft/battle_map.h"
#include "psx/types.h"

rgb24_t* map_get_background_color(void) {
    return &g_map_background_color;
}
