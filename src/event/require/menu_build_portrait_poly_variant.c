#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Texture a quad with a portrait: a unit portrait when neither bit 0x100 nor
 * 0x200 of `flags` is set, and the formation portrait named by the low byte
 * when bit 0x100 is set. */
void menu_build_portrait_poly_variant(s32 flags, POLY_FT4* output) {
    if ((flags & 0x300) == 0) {
        menu_build_unit_portrait_poly_variant(output, flags);
    }
    if ((flags & 0x100) != 0) {
        gfx_set_formation_icon_uv(output, flags & 0xff);
    }
}
