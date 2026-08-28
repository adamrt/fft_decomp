#include "fft/attack.h"
#include "fft/battle.h"
#include "fft/main_sound.h"
#include "psx/pad.h"
#include "psx/types.h"

void attack_deploy_handle_roster_navigation_input(void) {
    u32* flags;

    flags = battle_script_get_controller_input_pointer(0);
    if ((*flags & PSX_PAD_L1) && (battle_thread_is_running(4) == 0)) {
        g_attack_panel_status_animation[0] = 7;
        g_sound_effect_id_to_play = MAIN_SFX_PAGE_SWITCH;
        battle_thread_start(4, attack_deploy_select_previous_roster_unit);
    }
    if ((*flags & PSX_PAD_R1) && (battle_thread_is_running(4) == 0)) {
        g_attack_panel_status_animation[1] = 7;
        g_sound_effect_id_to_play = MAIN_SFX_PAGE_SWITCH;
        battle_thread_start(4, attack_deploy_select_next_roster_unit);
    }
}
