#include "fft/battle.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"

s32 battle_state_sync_frame(u32 ordering_table) {
    s32 wait;
    s32 sync_wait;
    s32 timer;
    s32 sync_result;

    if (g_animation_speed == 1) {
        DrawSync(0);
        sync_result = VSync(0);
    } else {
        DrawSync(0);
        if (g_battle_game_state == BATTLE_GAME_STATE_EFFECT
            || g_battle_game_state == BATTLE_GAME_STATE_ACTION_EXECUTE) {
            timer = g_frame_pacing_timer;
            if (timer >= 0x10) {
                wait = 4;
                g_frame_pacing_timer = timer - 1;
            } else if (timer != 0) {
                wait = 3;
                g_frame_pacing_timer = timer - 1;
            } else {
                wait = 2;
            }
            sync_wait = g_frame_pacing;
            if (wait < sync_wait) {
                if (sync_wait == 1) {
                    sync_wait = 0;
                }
            } else {
                sync_wait = wait;
            }
            sync_result = VSync(sync_wait);
        } else {
            sync_result = VSync(g_animation_speed);
        }
    }

    PutDispEnv(&g_main_gfx_display_envs[g_main_gfx_screen_polarity]);
    PutDrawEnv(&g_main_gfx_draw_envs[g_main_gfx_screen_polarity]);
    DrawOTag((u32*)ordering_table);
    FntFlush(-1);
    return sync_result;
}
