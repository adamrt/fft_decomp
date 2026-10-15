#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/menu.h"

void menu_init_subsystems(void) {
    menu_init_subsystem_pointers();
    menu_init_palette_and_text(0);
    menu_init_stub();
    menu_reset_subsystems();
    map_reset_script_light_transform_variant();
    menu_init_panel_fade_tiles();
}
