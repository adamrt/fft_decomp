#include "fft/world.h"

/* Empty in WORLD; BATTLE's body loads the result image of the previewed action into the unit panel.
 * Called with gfx_init_image_loading's arguments. It leaves $v0 unset; the shared caller discards
 * the result. */
s32 menu_display_projected_action_effect_variant(POLY_FT4* unused_primitive, const point16_t* unused_base_load,
    const RECT* unused_base_screen, image_load_parameters_t* unused_parameters) {
}
