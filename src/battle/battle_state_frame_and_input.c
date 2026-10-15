#include "fft/battle.h"
#include "fft/battle_script.h"
#include "fft/script.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Read pad 1 and derive the frame's pressed and released buttons; always returns
 * 1.
 *
 * While script variable EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS is set the
 * event script filters the raw buttons first. The 32-bit pressed and previous button values of
 * the frame before are kept in four copies each, and g_main_input_stable_frames
 * counts the frames the buttons have not changed. Triangle outside the event
 * state requests the status menu, with its sound, when the menu is enabled, not
 * yet requested and the source unit is not player controlled. Holding Start,
 * Select, L1 and R1 together resets the game. */
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

/* Read pad 1 for the deployment screen and derive the pressed and released
 * buttons; the reset button combination resets the game. Always returns 1, so the
 * -1 that battle_state_run_deployment tests for does not occur. */
s32 battle_state_update_deployment_controller_input(void) {
    u32 previous;
    u32 current;

    current = PadRead(1);
    previous = g_input_previous_buttons;
    g_input_current_buttons = current;
    g_input_previous_buttons = current;
    g_main_input_pressed_buttons = ~previous & current;
    g_main_input_released_buttons = ~current & previous;

    if ((current & MAIN_INPUT_RESET_BUTTONS) == MAIN_INPUT_RESET_BUTTONS) {
        main_system_reset_game();
    }
    return 1;
}

/* Reset the GPU and set up the two frame buffers for a width x height battle
 * screen with the given projection distance and background color.
 *
 * The second buffer lies at y 0xf0 unless the height is 0x1e0. The draw
 * environments start at x -0x80, so primitive x 0x80 is the left screen edge;
 * dithering is on and the automatic background clear is off. The area of
 * g_battle_deployment_clear_rect is cleared to black and, when font printing is
 * enabled, main_gfx_load_efc_fnt loads the print font again.
 * battle_state_run_deployment calls this with 256 x 240 and projection 0x200. */
void battle_state_init_deployment_display(s32 width, s32 height, s32 projection, u8 red, u8 green, u8 blue) {
    RECT clear_rect;
    s32 second_y;

    clear_rect = g_battle_deployment_clear_rect;
    second_y = -(height != 0x1e0) & 0xf0;

    SetDispMask(0);
    ResetGraph(1);
    SetGeomOffset(width / 2, height / 2);
    SetGeomScreen(projection);

    SetDefDrawEnv(&g_main_gfx_draw_envs[0], -0x80, 0, width + 0x80, height);
    SetDefDispEnv(&g_main_gfx_display_envs[0], 0, second_y, width, height);
    SetDefDrawEnv(&g_main_gfx_draw_envs[1], -0x80, second_y, width + 0x80, height);
    SetDefDispEnv(&g_main_gfx_display_envs[1], 0, 0, width, height);

    g_main_gfx_draw_envs[0].dtd = g_main_gfx_draw_envs[1].dtd = 1;
    g_main_gfx_draw_envs[0].isbg = g_main_gfx_draw_envs[1].isbg = 0;
    setRGB0(&g_main_gfx_draw_envs[0], red, green, blue);
    setRGB0(&g_main_gfx_draw_envs[1], red, green, blue);

    ClearImage(&clear_rect, 0, 0, 0);
    DrawSync(0);
    if (g_main_font_print_enabled != 0) {
        main_gfx_load_efc_fnt();
    }
    PutDispEnv(&g_main_gfx_display_envs[g_main_gfx_screen_polarity]);
    PutDrawEnv(&g_main_gfx_draw_envs[g_main_gfx_screen_polarity]);
}

/* End a battle frame: wait for drawing and the vertical blank, flip the display
 * and draw environments and draw the ordering table. Returns VSync's result,
 * which the game loop keeps as the frame's hsync count.
 *
 * At animation speed 1 the wait is one vertical blank. Otherwise it is
 * g_main_system_animation_speed blanks, except in the effect and action-execute
 * states: there it is 2 blanks, or 3 while g_battle_frame_pacing_timer runs (4 while it
 * is 16 or more), with the timer counting down each frame, and g_battle_frame_pacing
 * replaces it when larger. The test for a g_battle_frame_pacing of 1 cannot succeed,
 * since the value it is compared with is at least 2. */
s32 battle_state_sync_frame(u32* ordering_table) {
    s32 wait;
    s32 sync_wait;
    s32 timer;
    s32 sync_result;

    if (g_main_system_animation_speed == 1) {
        DrawSync(0);
        sync_result = VSync(0);
    } else {
        DrawSync(0);
        if (g_battle_game_state == BATTLE_GAME_STATE_EFFECT
            || g_battle_game_state == BATTLE_GAME_STATE_ACTION_EXECUTE) {
            timer = g_battle_frame_pacing_timer;
            if (timer >= 0x10) {
                wait = 4;
                g_battle_frame_pacing_timer = timer - 1;
            } else if (timer != 0) {
                wait = 3;
                g_battle_frame_pacing_timer = timer - 1;
            } else {
                wait = 2;
            }
            sync_wait = g_battle_frame_pacing;
            if (wait < sync_wait) {
                if (sync_wait == 1) {
                    sync_wait = 0;
                }
            } else {
                sync_wait = wait;
            }
            sync_result = VSync(sync_wait);
        } else {
            sync_result = VSync(g_main_system_animation_speed);
        }
    }

    PutDispEnv(&g_main_gfx_display_envs[g_main_gfx_screen_polarity]);
    PutDrawEnv(&g_main_gfx_draw_envs[g_main_gfx_screen_polarity]);
    DrawOTag(ordering_table);
    FntFlush(-1);
    return sync_result;
}

/* End a frame of the deployment screen: wait for drawing and one vertical blank,
 * flip the display and draw environments and draw the ordering table. Returns
 * VSync's hsync count. */
s32 battle_state_sync_and_submit_deployment_frame(u32* ordering_table) {
    s32 sync_result;

    DrawSync(0);
    sync_result = VSync(0);
    PutDispEnv(&g_main_gfx_display_envs[g_main_gfx_screen_polarity]);
    PutDrawEnv(&g_main_gfx_draw_envs[g_main_gfx_screen_polarity]);
    DrawOTag(ordering_table);
    FntFlush(-1);
    return sync_result;
}
