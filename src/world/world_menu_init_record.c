#include "fft/world.h"
#include "psx/types.h"

void world_menu_init_record(void* record) {
    world_gfx_texture_window_record_t* view = record;

    view->sprite.w = 0;
    view->texture_window.h = 0;
    world_script_copy_bytes(&view->texture_window, g_world_menu_record_template, 8);
}
