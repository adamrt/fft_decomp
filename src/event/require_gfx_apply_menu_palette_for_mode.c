#include "fft/battle.h"
#include "psx/types.h"

void require_gfx_apply_menu_palette_for_mode(void* output, u8* context) {
    if ((*(s32*)(context + 0x10) == 1) || (g_event_mode == 1)) {
        battle_menu_init_primitive_colors_palette_bank_1((world_menu_palette_primitives_t*)output);
    } else {
        battle_menu_init_primitive_colors_palette_bank_0((world_menu_palette_primitives_t*)output);
    }
}
