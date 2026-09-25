#include "fft/event_attack.h"
#include "psx/gpu.h"

void attack_gfx_build_portrait_poly_ft4(s32 portrait_id, POLY_FT4* poly) {
    if ((portrait_id & 0x300) == 0) {
        battle_menu_build_unit_portrait_poly(poly, portrait_id);
    }
    if ((portrait_id & 0x200) != 0) {
        attack_gfx_set_formation_icon_uv(poly, portrait_id & 0xff);
    }
}
