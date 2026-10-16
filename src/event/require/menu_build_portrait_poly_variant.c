#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Texture a quad with a portrait: a unit portrait when neither bit 0x100 nor
 * 0x200 of `flags` is set, and the formation portrait named by the low byte
 * when bit 0x100 is set. */
void menu_build_portrait_poly_variant(s32 portrait_id, POLY_FT4* poly) {
    if ((portrait_id & MENU_PORTRAIT_CLUT_SUPPRESS_MASK) == 0) {
        menu_build_unit_portrait_poly_variant(poly, portrait_id);
    }
    if ((portrait_id & 0x100) != 0) {
        gfx_set_formation_icon_uv(poly, portrait_id & 0xff);
    }
}
