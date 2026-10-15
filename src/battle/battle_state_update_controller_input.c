#include "fft/battle.h"
#include "fft/battle_script.h"
#include "fft/script.h"
#include "psx/pad.h"
#include "psx/types.h"

s32 battle_state_update_controller_input(void) {
    u32 pressed;
    u32 previous;
    u32 current;
    u32 released;
    battle_unit_t* unit;

    g_input_current_buttons = PadRead(1);
    if (script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) != 0) {
        script_filter_controller_input(&g_input_current_buttons);
    }

    pressed = g_main_input_pressed_buttons;
    previous = g_input_previous_buttons;
    current = g_input_current_buttons;
    g_main_input_pressed_copy_0 = pressed;
    g_main_input_pressed_copy_1 = pressed;
    g_main_input_pressed_copy_2 = pressed;
    g_main_input_pressed_copy_3 = pressed;
    g_main_input_pressed_buttons = ~previous & current;
    released = ~current & previous;
    g_main_input_previous_copy_0 = previous;
    g_main_input_previous_copy_1 = previous;
    g_main_input_previous_copy_2 = previous;
    g_main_input_previous_copy_3 = previous;
    g_main_input_released_buttons = released;

    if (previous == current) {
        g_main_input_stable_frames++;
    } else {
        g_main_input_stable_frames = 0;
    }

    g_input_previous_buttons = g_input_current_buttons;
    if ((g_main_input_pressed_buttons & PSX_PAD_TRIANGLE) != 0 && g_battle_game_state != BATTLE_GAME_STATE_EVENT) {
        unit = battle_unit_get_source();
        if (g_battle_menu_status_enabled != 0 && g_battle_menu_status_requested == 0
            && (unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) == 0) {
            main_sound_play_sfx(MAIN_SFX_STATUS_MENU_REQUEST);
            g_battle_menu_status_requested = 1;
        }
    }

    if ((g_input_previous_buttons & (PSX_PAD_START | PSX_PAD_SELECT | PSX_PAD_L1 | PSX_PAD_R1))
        == (PSX_PAD_START | PSX_PAD_SELECT | PSX_PAD_L1 | PSX_PAD_R1)) {
        main_system_reset_game();
    }
    return 1;
}
