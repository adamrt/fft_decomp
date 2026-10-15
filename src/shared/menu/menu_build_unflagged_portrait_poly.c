#include "fft/battle_menu.h"
#include "fft/menu.h"

/* Texture a quad with a portrait: a unit portrait when neither bit 0x100 nor
 * 0x200 of `portrait_id` is set; otherwise the quad is left alone. */
void menu_build_unflagged_portrait_poly(s32 portrait_id, POLY_FT4* poly) {
    if ((portrait_id & 0x300) == 0) {
        menu_build_unit_portrait_poly_variant(poly, portrait_id);
    }
}
