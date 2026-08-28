#include "fft/battle.h"
#include "psx/gpu.h"
#include "psx/types.h"

void equip_gfx_build_portrait_poly_ft4(s32 portrait_id, void* poly) {
    if ((portrait_id & 0x300) == 0) {
        battle_menu_build_unit_portrait_poly(poly, portrait_id);
    }
}
