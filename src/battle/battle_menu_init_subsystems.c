#include "fft/battle.h"
#include "psx/types.h"

void battle_menu_init_subsystems(void) {
    battle_menu_init_subsystem_pointers();
    /* The target passes 0 to the argument-less callee. */
    ((void (*)(s32))battle_menu_load_palette_and_text_pointers)(0);
    battle_noop_8012e190();
    battle_menu_reset_subsystems();
    battle_camera_reset_script_transform();
    battle_gfx_init_scanline_frame();
}
