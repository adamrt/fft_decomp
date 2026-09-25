#include "fft/battle.h"

s32 battle_state_sync_and_submit_deployment_frame(u32 ordering_table) {
    s32 sync_result;

    DrawSync(0);
    sync_result = VSync(0);
    PutDispEnv(&g_main_gfx_display_envs[g_main_gfx_screen_polarity]);
    PutDrawEnv(&g_main_gfx_draw_envs[g_main_gfx_screen_polarity]);
    DrawOTag((u32*)ordering_table);
    FntFlush(-1);
    return sync_result;
}
