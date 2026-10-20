/* Battle and deployment input polling, display setup and frame submission. */

#include "fft/command.h"
#include "fft/deploy.h"
#include "fft/gfx.h"
#include "fft/input.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/state.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Input snapshots written by the controller poll; these copies have no identified reader. */
extern u32 g_input_pressed_copy_2;
extern u32 g_input_pressed_copy_3;
extern u32 g_input_previous_copy_1;
extern u32 g_input_previous_copy_2;
extern u32 g_input_previous_copy_3;

/* Read pad 1 for the battle loop: derive the pressed and released buttons and the stable-frame
 * count, keeping last frame's values in the input copies. Triangle requests the status menu on a
 * turn the player does not control; Start+Select+L1+R1 resets the game. Always returns 1. */
s32 state_update_controller_input(void) {
    u32 pressed;
    u32 previous;
    u32 current;
    u32 released;
    unit_runtime_t* unit;

    g_input_current_buttons = PadRead(1);
    if (script_variable_get(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) != 0) {
        script_filter_controller_input(&g_input_current_buttons);
    }

    pressed = g_input_pressed_buttons;
    previous = g_input_previous_buttons;
    current = g_input_current_buttons;
    g_input_pressed_copy_0 = pressed;
    g_input_pressed_copy_1 = pressed;
    g_input_pressed_copy_2 = pressed;
    g_input_pressed_copy_3 = pressed;
    g_input_pressed_buttons = ~previous & current;
    released = ~current & previous;
    g_input_previous_copy_0 = previous;
    g_input_previous_copy_1 = previous;
    g_input_previous_copy_2 = previous;
    g_input_previous_copy_3 = previous;
    g_input_released_buttons = released;

    if (previous == current) {
        g_input_stable_frames++;
    } else {
        g_input_stable_frames = 0;
    }

    g_input_previous_buttons = g_input_current_buttons;
    if ((g_input_pressed_buttons & PSX_PAD_TRIANGLE) != 0 && g_game_state != GAME_STATE_EVENT) {
        unit = unit_source_get();
        if (g_menu_status_enabled != 0 && g_menu_status_requested == 0
            && (unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) == 0) {
            sound_sfx_play(SOUND_SFX_STATUS_MENU_REQUEST);
            g_menu_status_requested = 1;
        }
    }

    if ((g_input_previous_buttons & INPUT_RESET_BUTTONS) == INPUT_RESET_BUTTONS) {
        system_game_reset();
    }
    return 1;
}

/* Read pad 1 for the deployment screen and derive the pressed and released
 * buttons; the reset button combination resets the game. Always returns 1, so the
 * -1 that map_load_run tests for does not occur. */
s32 deploy_update_controller_input(void) {
    u32 previous;
    u32 current = PadRead(1);

    previous = g_input_previous_buttons;
    g_input_current_buttons = current;
    g_input_previous_buttons = current;
    g_input_pressed_buttons = ~previous & current;
    g_input_released_buttons = ~current & previous;

    if ((current & INPUT_RESET_BUTTONS) == INPUT_RESET_BUTTONS) {
        system_game_reset();
    }
    return 1;
}

/* Reset the GPU and set up the two frame buffers for a width x height battle
 * screen with the given projection distance and background color.
 *
 * The second buffer lies at y 240 unless the height is 480. The draw
 * environments start at x -0x80, so primitive x 0x80 is the left screen edge;
 * dithering is on and the automatic background clear is off. The fixed
 * 256 x 480 area is cleared to black and, when font printing is
 * enabled, debug_font_load loads the print font again.
 * map_load_run calls this with 256 x 240 and projection 0x200. */
void deploy_init_display(s32 width, s32 height, s32 projection, u8 red, u8 green, u8 blue) {
    RECT clear_rect = { 0, 0, 256, 480 };
    s32 second_y = height != 480 ? 240 : 0;

    SetDispMask(0);
    ResetGraph(1);
    SetGeomOffset(width / 2, height / 2);
    SetGeomScreen(projection);

    SetDefDrawEnv(&g_gfx_draw_envs[0], -GFX_BATTLE_SCREEN_X, 0, width + GFX_BATTLE_SCREEN_X, height);
    SetDefDispEnv(&g_gfx_display_envs[0], 0, second_y, width, height);
    SetDefDrawEnv(&g_gfx_draw_envs[1], -GFX_BATTLE_SCREEN_X, second_y, width + GFX_BATTLE_SCREEN_X, height);
    SetDefDispEnv(&g_gfx_display_envs[1], 0, 0, width, height);

    g_gfx_draw_envs[0].dtd = g_gfx_draw_envs[1].dtd = 1;
    g_gfx_draw_envs[0].isbg = g_gfx_draw_envs[1].isbg = 0;
    setRGB0(&g_gfx_draw_envs[0], red, green, blue);
    setRGB0(&g_gfx_draw_envs[1], red, green, blue);

    ClearImage(&clear_rect, 0, 0, 0);
    DrawSync(0);
    if (g_debug_font_print_enabled != 0) {
        debug_font_load();
    }
    PutDispEnv(&g_gfx_display_envs[g_gfx_screen_polarity]);
    PutDrawEnv(&g_gfx_draw_envs[g_gfx_screen_polarity]);
}

/* End a battle frame: wait for drawing and the vertical blank, flip the display
 * and draw environments and draw the ordering table. Returns VSync's result,
 * which the game loop keeps as the frame's hsync count.
 *
 * At vsync interval 1 the wait is one vertical blank. Otherwise it is
 * g_system_vsync_interval blanks, except in the effect and action-execute
 * states: there it is 2 blanks, or 3 while g_system_frame_pacing_timer runs (4 while it
 * is 16 or more), with the timer counting down each frame, and g_system_min_vsync_interval
 * replaces it when larger. The test for a g_system_min_vsync_interval of 1 cannot succeed,
 * since the value it is compared with is at least 2. */
s32 state_sync_frame(u32* ordering_table) {
    s32 wait;
    s32 sync_wait;
    s32 timer;
    s32 sync_result;

    if (g_system_vsync_interval == 1) {
        DrawSync(0);
        sync_result = VSync(0);
    } else {
        DrawSync(0);
        if (g_game_state == GAME_STATE_EFFECT || g_game_state == GAME_STATE_ACTION_EXECUTE) {
            timer = g_system_frame_pacing_timer;
            if (timer >= 0x10) {
                wait = 4;
                g_system_frame_pacing_timer = timer - 1;
            } else if (timer != 0) {
                wait = 3;
                g_system_frame_pacing_timer = timer - 1;
            } else {
                wait = 2;
            }
            sync_wait = g_system_min_vsync_interval;
            if (wait < sync_wait) {
                if (sync_wait == 1) {
                    sync_wait = 0;
                }
            } else {
                sync_wait = wait;
            }
            sync_result = VSync(sync_wait);
        } else {
            sync_result = VSync(g_system_vsync_interval);
        }
    }

    PutDispEnv(&g_gfx_display_envs[g_gfx_screen_polarity]);
    PutDrawEnv(&g_gfx_draw_envs[g_gfx_screen_polarity]);
    DrawOTag(ordering_table);
    FntFlush(-1);
    return sync_result;
}

/* End a frame of the deployment screen: wait for drawing and one vertical blank,
 * flip the display and draw environments and draw the ordering table. Returns
 * VSync's hsync count. */
s32 deploy_sync_and_submit_frame(u32* ordering_table) {
    s32 sync_result;

    DrawSync(0);
    sync_result = VSync(0);
    PutDispEnv(&g_gfx_display_envs[g_gfx_screen_polarity]);
    PutDrawEnv(&g_gfx_draw_envs[g_gfx_screen_polarity]);
    DrawOTag(ordering_table);
    FntFlush(-1);
    return sync_result;
}
