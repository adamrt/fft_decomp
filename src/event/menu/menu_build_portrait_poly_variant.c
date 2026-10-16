#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Texture a quad with a portrait: a unit portrait when neither bit 0x100 nor
 * 0x200 of `portrait_id` is set; otherwise the quad is left alone. */
void menu_build_portrait_poly_variant(s32 portrait_id, POLY_FT4* poly) {
    if ((portrait_id & MENU_PORTRAIT_CLUT_SUPPRESS_MASK) == 0) {
        menu_build_unit_portrait_poly_variant(poly, portrait_id);
    }
}
