#include "fft/battle.h"
#include "fft/battle_text.h"

s32 battle_action_report_job_level_up(battle_unit_misc_data_t* misc) {
    if (misc->action_rewards.job_level_for_display != 0) {
        u8 battle_id;
        battle_text_set_message_duration_frames(0xB4);
        battle_id = misc->battle_data->misc_unit_id;
        battle_menu_init_system_function(0xA, 0x183E, battle_id, battle_id, 1);
        battle_unit_set_level_up_animation(misc);
        battle_target_move_cursor_to_unit(misc);
        battle_effect_set_secondary_level_up(misc);
        main_sound_play_tune(3);
        return 1;
    }
    return 0;
}

/* padding */
