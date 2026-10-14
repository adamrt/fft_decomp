#include "fft/battle_map.h"

/* Save the map background color in the backup that
 * map_restore_background_color reloads. */
void map_copy_background_color(void) {
    /* Component copies retain the original byte loads and separate addresses. */
    g_map_background_color_backup.r = g_map_background_color.r;
    g_map_background_color_backup.g = g_map_background_color.g;
    g_map_background_color_backup.b = g_map_background_color.b;
}
