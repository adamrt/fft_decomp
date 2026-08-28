#include "fft/battle.h"
#include "fft/main_runtime.h"
#include "fft/script_variables.h"
#include "psx/pad.h"

s32 battle_state_update_controller_input(void) {
    u32 pressed;
    u32 previous;
    u32 current;
    u32 released;
    battle_unit_misc_data_t* unit;

    g_controller_input_raw = PadRead(1);
    if (battle_script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) != 0) {
        battle_script_filter_controller_input((u32*)&g_controller_input_raw);
    }

    pressed = g_controller_input_pressed;
    previous = g_controller_input_previous;
    current = g_controller_input_raw;
    g_controller_pressed_copy_0 = pressed;
    g_controller_pressed_copy_1 = pressed;
    g_controller_pressed_copy_2 = pressed;
    g_controller_pressed_copy_3 = pressed;
    g_controller_input_pressed = ~previous & current;
    released = ~current & previous;
    g_controller_previous_copy_0 = previous;
    g_controller_previous_copy_1 = previous;
    g_controller_previous_copy_2 = previous;
    g_controller_previous_copy_3 = previous;
    g_controller_input_released = released;

    if (previous == current) {
        g_controller_stable_frames++;
    } else {
        g_controller_stable_frames = 0;
    }

    g_controller_input_previous = g_controller_input_raw;
    if ((g_controller_input_pressed & PSX_PAD_TRIANGLE) != 0 && g_battle_game_state != BATTLE_GAME_STATE_EVENT) {
        unit = battle_unit_get_source_misc_data();
        if (g_battle_menu_status_enabled != 0 && g_battle_menu_status_requested == 0
            && (unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) == 0) {
            main_sound_play_sfx(0x71);
            g_battle_menu_status_requested = 1;
        }
    }

    if ((g_controller_input_previous & (PSX_PAD_START | PSX_PAD_SELECT | PSX_PAD_L1 | PSX_PAD_R1))
        == (PSX_PAD_START | PSX_PAD_SELECT | PSX_PAD_L1 | PSX_PAD_R1)) {
        main_system_reset_game();
    }
    return 1;
}
