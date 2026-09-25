#include "fft/world.h"
#include "psx/types.h"

void world_menu_init_record(world_gfx_texture_window_record_t* record) {
    record->sprite.w = 0;
    record->texture_window.h = 0;
    world_script_copy_bytes(&record->texture_window, g_world_menu_record_template, 8);
}
