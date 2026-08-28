#include "fft/battle.h"
#include "fft/main_sound.h"
#include "psx/types.h"

void battle_sound_set_effect_to_confirm_checked(void) {
    if (g_battle_menu_restore_pending == 0) {
        g_sound_effect_id_to_play = MAIN_SFX_CONFIRM;
    }
}
