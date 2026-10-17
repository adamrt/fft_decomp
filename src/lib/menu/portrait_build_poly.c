/* Portrait quads, with formation icons supported by ATTACK and REQUIRE. */

#include "fft/gfx_primitive.h"
#include "fft/menu.h"
#include "fft/portrait.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* ATTACK marks formation portraits with bit 0x200; REQUIRE uses bit 0x100. */
#ifdef FFT_EVENT_ATTACK
enum { PORTRAIT_FORMATION_ICON = 0x200 };
#endif
#ifdef FFT_EVENT_REQUIRE
enum { PORTRAIT_FORMATION_ICON = 0x100 };
#endif

/* Other menu overlays leave the quad alone when either suppress bit is set. */
void portrait_build_poly(s32 portrait_id, POLY_FT4* poly) {
    if ((portrait_id & MENU_PORTRAIT_CLUT_SUPPRESS_MASK) == 0) {
        portrait_build_unit_poly(poly, portrait_id);
    }
#if defined(FFT_EVENT_ATTACK) || defined(FFT_EVENT_REQUIRE)
    if ((portrait_id & PORTRAIT_FORMATION_ICON) != 0) {
        gfx_set_formation_icon_uv(poly, portrait_id & 0xff);
    }
#endif
}
