#include "fft/battle.h"
#include "fft/battle_menu_status_panel.h"
#include "psx/types.h"

void debugchr_gfx_apply_menu_palette_for_mode(
    world_menu_palette_primitives_t* output, const battle_menu_status_panel_frame_config_t* context) {
    if ((context->style == 1) || (g_event_mode == 1)) {
        battle_menu_init_primitive_colors_palette_bank_1(output);
    } else {
        battle_menu_init_primitive_colors_palette_bank_0(output);
    }
}
