#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "psx/types.h"

/* Reset the map palette state at step 0 of the map init sequences: no upload or
 * move pending, every row open to updates with its sixteen CLUT animations
 * stopped, the background gradient and ambient light fades and their repeats off,
 * and the eight color rotation slots free.
 *
 * Each row's `enabled` is set, which makes map_update_animations blend the
 * row's 256 colors as one under animation 0. The sprite, unit, unit shadow and
 * action result rows get 0 and blend each of their sixteen CLUTs on its own. Of
 * the stored deltas only the first color's red delta of each row is cleared. */
void map_init_palette_state(void) {
    s32 clut_row;
    s32 animation_index;

    g_map_palette_state.upload_pending = 0;
    g_map_palette_state.move_pending = 0;

    for (clut_row = 0; clut_row < MAP_CLUT_ROW_COUNT; clut_row++) {
        g_map_palette_state.clut_rows[clut_row].updates_disabled = 0;
        g_map_palette_state.clut_rows[clut_row].enabled = 1;

        for (animation_index = MAP_CLUTS_PER_ROW - 1; animation_index >= 0; animation_index--) {
            g_map_palette_state.clut_rows[clut_row].animations[animation_index].active = 0;
        }

        g_map_palette_state.clut_rows[clut_row].components[0].delta_biased.r = 0;
    }

    g_map_background_gradient_transition.active = 0;
    g_map_background_gradient_transition.repeat = MAP_COLOR_REPEAT_OFF;
    g_map_ambient_light_transition.active = 0;
    g_map_ambient_light_transition.repeat = MAP_COLOR_REPEAT_OFF;
    g_map_palette_state.clut_rows[MAP_CLUT_ROW_SPRITES].enabled = 0;
    g_map_palette_state.clut_rows[MAP_CLUT_ROW_UNITS].enabled = 0;
    g_map_palette_state.clut_rows[MAP_CLUT_ROW_UNIT_SHADOWS].enabled = 0;
    g_map_palette_state.clut_rows[MAP_CLUT_ROW_ACTION_RESULTS].enabled = 0;
    map_reset_clut_cycles();
}
