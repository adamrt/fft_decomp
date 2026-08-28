#include "fft/require.h"
#include "psx/gpu.h"
#include "psx/types.h"

void require_gfx_build_portrait_poly_ft4(s32 flags, void* output) {
    if ((flags & 0x300) == 0) {
        battle_menu_build_unit_portrait_poly(output, flags);
    }
    if ((flags & 0x100) != 0) {
        require_gfx_set_formation_icon_uv(output, flags & 0xff);
    }
}
