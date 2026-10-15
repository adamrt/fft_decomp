#include "fft/battle.h"
#include "fft/battle_script.h"
#include "fft/script.h"
#include "psx/pad.h"
#include "psx/types.h"

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
