#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/world.h"

/*
 * Update the menu panel's fade and submit its center and edge tiles.
 *
 * Mode 1 raises intensity toward 80; mode 2 lowers it toward 0. The nine
 * tile pairs taper away from the center in increments of 8.
 * BATTLE twin of world_menu_update_panel_fade.
 */
void battle_menu_update_panel_fade(void) {
    world_fade_tile_frame_t* frame;
    s32 i;
    s32 intensity;

    if (g_battle_menu_panel_fade_mode != 0) {
        frame = &g_battle_menu_panel_fade_frames[g_battle_menu_packet_buffer_index];
        if (g_battle_menu_panel_fade_mode == 1 && g_battle_menu_panel_fade_intensity < 80) {
            g_battle_menu_panel_fade_intensity += 8;
        }
        if (g_battle_menu_panel_fade_mode == 2) {
            if (g_battle_menu_panel_fade_intensity != 0) {
                g_battle_menu_panel_fade_intensity -= 8;
            } else {
                g_battle_menu_panel_fade_mode = 0;
            }
        }
        frame->center.r0 = ((world_fade_intensity_t*)&g_battle_menu_panel_fade_intensity)->bytes[0];
        frame->center.g0 = ((world_fade_intensity_t*)&g_battle_menu_panel_fade_intensity)->bytes[0];
        frame->center.b0 = ((world_fade_intensity_t*)&g_battle_menu_panel_fade_intensity)->bytes[0];
        intensity = g_battle_menu_panel_fade_intensity;
        for (i = 0; i < 9; i++) {
            if (intensity != 0) {
                intensity -= 8;
            }
            /* Pointer walks preserve the target's two biased tile indices. */
            (frame->sides + (8 - i))->r0 = intensity;
            (frame->sides + (8 - i))->g0 = intensity;
            (frame->sides + (8 - i))->b0 = intensity;
            (frame->sides + i + 9)->r0 = intensity;
            (frame->sides + i + 9)->g0 = intensity;
            (frame->sides + i + 9)->b0 = intensity;
            battle_gfx_draw_or_append_gpu_primitive((s32*)&frame->sides[8 - i]);
            battle_gfx_draw_or_append_gpu_primitive((s32*)&frame->sides[i + 9]);
        }
        battle_gfx_draw_or_append_gpu_primitive(
            (s32*)&g_battle_menu_panel_fade_frames[g_battle_menu_packet_buffer_index].center);
        battle_gfx_draw_or_append_gpu_primitive(
            (s32*)&g_battle_menu_panel_fade_frames[g_battle_menu_packet_buffer_index].draw_mode);
    }
}
