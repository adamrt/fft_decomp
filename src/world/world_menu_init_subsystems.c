#include "fft/world.h"

void world_menu_init_subsystems(void) {
    world_init_scene_bindings();
    world_menu_init_palette_and_text(0);
    world_noop_800e2390();
    world_menu_reset_runtime();
    world_noop_800f29d8();
    world_menu_init_panel_fade_tiles();
}
