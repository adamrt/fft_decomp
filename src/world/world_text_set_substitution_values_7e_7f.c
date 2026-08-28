#include "fft/world.h"
#include "psx/types.h"

/* Store the four-halfword WORLD graphic rectangle state. */
void world_text_set_substitution_values_7e_7f(s16 x, s16 y, s16 width, s16 height) {
    g_world_text_substitution_rect.x = x;
    g_world_text_substitution_rect.y = y;
    g_world_text_substitution_rect.w = width;
    g_world_text_substitution_rect.h = height;
}
