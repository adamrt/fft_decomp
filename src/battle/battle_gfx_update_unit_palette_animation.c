#include "fft/battle.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

void battle_gfx_update_unit_palette_animation(void) {
    s32 dst;
    s32 src;
    u8* palette;
    s32 unused[2];

    g_battle_unit_palette_animation_timer += g_animation_speed;
    if (g_battle_unit_palette_animation_timer >= 4) {
        g_battle_unit_palette_animation_timer = 0;
        g_battle_unit_palette_animation_offset++;
        if (g_battle_unit_palette_animation_offset >= 15) {
            g_battle_unit_palette_animation_offset = 0;
        }
        if (g_battle_unit_palette_animation_offset != 0) {
            /* dst before src: the reverse order changes the copy loop registers. */
            dst = 2;
            src = g_battle_unit_palette_animation_offset * 2 + 2;
            g_battle_gfx_cycled_deployed_palettes[0][0] = ((u8*)g_battle_deployed_unit_palettes)[0];
            g_battle_gfx_cycled_deployed_palettes[0][1] = ((u8*)g_battle_deployed_unit_palettes)[1];
            g_battle_gfx_cycled_deployed_palettes[1][0] = ((u8*)g_battle_deployed_unit_palettes[1])[0];
            g_battle_gfx_cycled_deployed_palettes[1][1] = ((u8*)g_battle_deployed_unit_palettes[1])[1];
            for (; src < 32; src++, dst++) {
                g_battle_gfx_cycled_deployed_palettes[0][dst] = ((u8*)g_battle_deployed_unit_palettes)[src];
                g_battle_gfx_cycled_deployed_palettes[1][dst] = ((u8*)g_battle_deployed_unit_palettes[1])[src];
            }
            for (src = 2, dst = 32 - g_battle_unit_palette_animation_offset * 2;
                src < g_battle_unit_palette_animation_offset * 2 + 2; src++, dst++) {
                g_battle_gfx_cycled_deployed_palettes[0][dst] = ((u8*)g_battle_deployed_unit_palettes)[src];
                g_battle_gfx_cycled_deployed_palettes[1][dst] = ((u8*)g_battle_deployed_unit_palettes[1])[src];
            }
            palette = g_battle_gfx_cycled_deployed_palettes[0];
        } else {
            palette = (u8*)g_battle_deployed_unit_palettes;
        }
        battle_map_load_palette_data((const u16*)palette, 2, 1, 0);
        battle_map_load_palette_data((const u16*)(palette + 32), 2, 2, 0);
        (void)&unused;
    }
}
