#include "fft/battle_ai.h"

s32 battle_ai_save_fading_life_auto_battle(void) {
    if (g_battle_ai_data_base.decision_state == 0) {
        if (battle_ai_select_peril_target() != 0) {
            g_battle_ai_data_base.autobattle_setting = BATTLE_AI_AUTOBATTLE_RECOVERY;
            return -2;
        }
        battle_ai_store_main_target_id_and_focus_on_target_flag(g_battle_ai_temp_unit_data->auto_battle_target);
    }
    return -(~battle_ai_run_non_specific_autobattle() == 0);
}
