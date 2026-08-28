#include "fft/world.h"
#include "psx/types.h"

/* Fetch one of the nine menu-script streams by index. */
u8* world_menu_get_script(s32 index) {
    return g_world_menu_scripts[index];
}
