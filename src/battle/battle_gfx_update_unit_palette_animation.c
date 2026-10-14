#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "psx/types.h"

/* Cycles the colors of the two deployed-unit palettes (sprite CLUTs 1 and 2).
 *
 * Every fourth animation tick the offset advances through 0..14 and the palettes
 * are uploaded again with colors 1..15 rotated down by that many places; color
 * 0 stays in place, and offset 0 uploads the palettes as stored. */
void battle_gfx_update_unit_palette_animation(void) {
    s32 dst;
    s32 src;
    palette_t* palettes;
    RECT rect; /* never used; retail reserves its 8 bytes */

    g_battle_unit_palette_animation_timer += g_main_system_animation_speed;
    if (g_battle_unit_palette_animation_timer >= 4) {
        g_battle_unit_palette_animation_timer = 0;
        g_battle_unit_palette_animation_offset++;
        if (g_battle_unit_palette_animation_offset >= 15) {
            g_battle_unit_palette_animation_offset = 0;
        }
        if (g_battle_unit_palette_animation_offset != 0) {
            /* Bytewise rotation retains the native stores and first-color 16-bit load. */
            /* dst before src: the reverse order changes the copy loop registers. */
            dst = 2;
            src = g_battle_unit_palette_animation_offset * 2 + 2;
            ((u8*)g_battle_gfx_cycled_deployed_palettes[0])[0] = ((u8*)g_battle_deployed_unit_palettes)[0];
            ((u8*)g_battle_gfx_cycled_deployed_palettes[0])[1] = ((u8*)g_battle_deployed_unit_palettes)[1];
            ((u8*)g_battle_gfx_cycled_deployed_palettes[1])[0] = *(u16*)&g_battle_deployed_unit_palettes[1][0];
            ((u8*)g_battle_gfx_cycled_deployed_palettes[1])[1] = ((u8*)g_battle_deployed_unit_palettes[1])[1];
            /* Colors offset + 1..15 move down to 1.., and colors 1..offset wrap to the top. */
            for (; src < 32; src++, dst++) {
                ((u8*)g_battle_gfx_cycled_deployed_palettes[0])[dst] = ((u8*)g_battle_deployed_unit_palettes)[src];
                ((u8*)g_battle_gfx_cycled_deployed_palettes[1])[dst] = ((u8*)g_battle_deployed_unit_palettes[1])[src];
            }
            for (src = 2, dst = 32 - g_battle_unit_palette_animation_offset * 2;
                src < g_battle_unit_palette_animation_offset * 2 + 2; src++, dst++) {
                ((u8*)g_battle_gfx_cycled_deployed_palettes[0])[dst] = ((u8*)g_battle_deployed_unit_palettes)[src];
                ((u8*)g_battle_gfx_cycled_deployed_palettes[1])[dst] = ((u8*)g_battle_deployed_unit_palettes[1])[src];
            }
            palettes = g_battle_gfx_cycled_deployed_palettes;
        } else {
            palettes = g_battle_deployed_unit_palettes;
        }
        map_load_palette_data(palettes, MAP_CLUT_ROW_SPRITES, 1, 0);
        map_load_palette_data(palettes + 1, MAP_CLUT_ROW_SPRITES, 2, 0);
    }
}
