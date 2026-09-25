#include "fft/world.h"
#include "psx/types.h"

void world_sound_set_effect_to_confirm_checked(void) {
    if (g_world_menu_restore_pending == 0) {
        g_world_sound_effect_id_to_play = MAIN_SFX_CONFIRM;
    }
}
