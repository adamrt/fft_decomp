#include "fft/world.h"

/* Choose the CLUT bank for the four menu icon sprites: the highlighted set
 * while another thread is running or the task thread is active. */
void world_menu_select_icon_cluts(world_menu_icon_sprites_t* menu) {
    if (world_thread_is_previous_running() != 0 || g_world_thread_task_active == 1) {
        menu->sprites[0].clut = 0x7D3C;
        menu->sprites[1].clut = 0x7DFC;
        menu->sprites[2].clut = 0x7E3C;
        menu->sprites[3].clut = 0x7C7C;
    } else {
        menu->sprites[0].clut = 0x7C3C;
        menu->sprites[1].clut = 0x7D7C;
        menu->sprites[2].clut = 0x7DBC;
        menu->sprites[3].clut = 0x7CBC;
    }
}
