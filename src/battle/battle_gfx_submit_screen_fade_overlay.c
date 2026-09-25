#include "fft/battle.h"
#include "psx/gpu.h"
#include "psx/types.h"

void battle_gfx_submit_screen_fade_overlay(battle_gfx_fade_overlay_t* overlay, s32 level) {
    s32 blend;

    if (level != 0) {
        SetTile(&overlay->tile);
        SetSemiTrans(&overlay->tile, 1);
        blend = 2;
        overlay->tile.w = 256;
        overlay->tile.h = 240;
        overlay->tile.r0 = 0;
        overlay->tile.g0 = 0;
        overlay->tile.b0 = 0;
        overlay->tile.x0 = 128;
        overlay->tile.y0 = 0;
        if (level < 0) {
            blend = 1;
            level = -level;
        }
        SetDrawMode(&overlay->mode, 0, 0, GetTPage(0, blend, 0x3c0, 0x100), &g_battle_menu_disabled_texture_window);
        overlay->tile.r0 = level;
        overlay->tile.g0 = level;
        overlay->tile.b0 = level;
        if (g_battle_etc_graphics_enabled != 0) {
            battle_gfx_draw_or_append_gpu_primitive(&overlay->mode);
            battle_gfx_draw_or_append_gpu_primitive(&overlay->tile);
        } else {
            battle_gfx_draw_or_append_gpu_primitive(&overlay->tile);
            battle_gfx_draw_or_append_gpu_primitive(&overlay->mode);
        }
        g_battle_script_event_input = 0;
    }
}
