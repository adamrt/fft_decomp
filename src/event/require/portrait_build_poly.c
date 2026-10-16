/* Texturing a quad with a unit portrait or, when the portrait id says so, a formation portrait. */

#include "fft/gfx_primitive.h"
#include "fft/menu.h"
#include "fft/portrait.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* In REQUIRE this portrait id bit names a formation portrait by the id's low byte; ATTACK's copy
 * tests 0x200 instead. */
enum { PORTRAIT_FORMATION_ICON = 0x100 };

/* Texture a quad with a portrait: a unit portrait when neither bit of
 * MENU_PORTRAIT_CLUT_SUPPRESS_MASK is set in `portrait_id`, and the formation
 * portrait named by the low byte when PORTRAIT_FORMATION_ICON is set. */
void portrait_build_poly(s32 portrait_id, POLY_FT4* poly) {
    if ((portrait_id & MENU_PORTRAIT_CLUT_SUPPRESS_MASK) == 0) {
        portrait_build_unit_poly(poly, portrait_id);
    }
    if ((portrait_id & PORTRAIT_FORMATION_ICON) != 0) {
        gfx_set_formation_icon_uv(poly, portrait_id & 0xff);
    }
}
