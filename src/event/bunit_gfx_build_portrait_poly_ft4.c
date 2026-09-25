#include "fft/event_bunit.h"
#include "psx/gpu.h"

void bunit_gfx_build_portrait_poly_ft4(s32 portrait_id, POLY_FT4* poly) {
    if ((portrait_id & 0x300) == 0) {
        battle_menu_build_unit_portrait_poly(poly, portrait_id);
    }
}
