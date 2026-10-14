#include "fft/menu.h"
#include "psx/types.h"

/* Store the rectangle text variables $7E and $7F substitute. */
void text_set_substitution_values_7e_7f(s32 x, s32 y, s32 width, s32 height) {
    g_text_substitution_rect.x = x;
    g_text_substitution_rect.y = y;
    g_text_substitution_rect.w = width;
    g_text_substitution_rect.h = height;
}
