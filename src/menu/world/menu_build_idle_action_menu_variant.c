#include "fft/world.h"
#include "psx/types.h"

/* Clear the menu-building-started flag; the menu id is not read. */
void menu_build_idle_action_menu_variant(s32 unused_menu_id) {
    g_menu_building_thread_started = 0;
}
