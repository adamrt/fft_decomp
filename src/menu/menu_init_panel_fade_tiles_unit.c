#include "fft/battle_menu.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "psx/libgpu.h"
#include "psx/types.h"

void menu_init_panel_fade_tiles(void) {
    menu_gradient_panel_prims_t* frame;
    s32 i;

    frame = &g_menu_panel_fade_frames[0];

    SetDrawMode(&frame->draw_mode, 0, 0, GetTPage(0, 2, 0x3c0, 0x100), &g_gfx_texture_window);

    SetTile(&frame->center);
    setRGB0(&frame->center, 0, 0, 0);
    SetSemiTrans(&frame->center, 1);
    frame->center.x0 = 0x80;
    frame->center.y0 = 0xac;
    frame->center.w = 0x100;
    frame->center.h = 0x32;

    for (i = 0; i < 9; i++) {
        SetTile(&frame->top[i]);
        setRGB0(&frame->top[i], 0, 0, 0);
        SetSemiTrans(&frame->top[i], 1);
        frame->top[i].w = 0x100;
        frame->top[i].h = 1;
        frame->top[i].x0 = 0x80;
        frame->top[i].y0 = i + 0xa3;

        SetTile(&frame->bottom[i]);
        setRGB0(&frame->bottom[i], 0, 0, 0);
        SetSemiTrans(&frame->bottom[i], 1);
        frame->bottom[i].w = 0x100;
        frame->bottom[i].h = 1;
        frame->bottom[i].x0 = 0x80;
        frame->bottom[i].y0 = i + 0xde;
    }

    mem_copy_bytes_nonempty(
        &g_menu_panel_fade_frames[1], &g_menu_panel_fade_frames[0], sizeof(menu_gradient_panel_prims_t));
}

/* Update panel brightness and submit the center and edge tiles. */
void menu_update_panel_fade(void) {
    menu_gradient_panel_prims_t* frame;
    s32 i;
    s32 intensity;

    if (g_menu_panel_fade_mode != MENU_PANEL_FADE_NONE) {
        frame = &g_menu_panel_fade_frames[g_menu_packet_buffer_index];
        if (g_menu_panel_fade_mode == MENU_PANEL_FADE_IN && g_menu_panel_fade_intensity < 80) {
            g_menu_panel_fade_intensity += 8;
        }
        if (g_menu_panel_fade_mode == MENU_PANEL_FADE_OUT) {
            if (g_menu_panel_fade_intensity != 0) {
                g_menu_panel_fade_intensity -= 8;
            } else {
                g_menu_panel_fade_mode = MENU_PANEL_FADE_NONE;
            }
        }
        setRGB0(&frame->center, g_menu_panel_fade_intensity, g_menu_panel_fade_intensity, g_menu_panel_fade_intensity);
        intensity = g_menu_panel_fade_intensity;
        for (i = 0; i < 9; i++) {
            if (intensity != 0) {
                intensity -= 8;
            }
            /* The pointer walk preserves the target's operand order for the biased index. */
            (frame->top + (8 - i))->r0 = intensity;
            (frame->top + (8 - i))->g0 = intensity;
            (frame->top + (8 - i))->b0 = intensity;
            setRGB0(&frame->bottom[i], intensity, intensity, intensity);
            gfx_draw_or_append_gpu_primitive(&frame->top[8 - i]);
            gfx_draw_or_append_gpu_primitive(&frame->bottom[i]);
        }
        gfx_draw_or_append_gpu_primitive(&g_menu_panel_fade_frames[g_menu_packet_buffer_index].center);
        gfx_draw_or_append_gpu_primitive(&g_menu_panel_fade_frames[g_menu_packet_buffer_index].draw_mode);
    }
}

/* Draws a full-screen semi-transparent tile of gray level |level|: positive
 * values subtract (abr 2), negative values add (abr 1), and 0 draws nothing.
 *
 * The draw mode is submitted
 * before the tile while an ETC full-screen graphic draws primitives immediately,
 * and after it when both are linked into the ordering table. A non-zero level
 * also clears g_script_event_input, so menus see no button while the
 * screen is faded. */
void gfx_submit_screen_fade_overlay(fade_overlay_t* overlay, s32 level) {
    s32 blend;

    if (level != 0) {
        SetTile(&overlay->tile);
        SetSemiTrans(&overlay->tile, 1);
        blend = 2;
        overlay->tile.w = 256;
        overlay->tile.h = 240;
        setRGB0(&overlay->tile, 0, 0, 0);
        overlay->tile.x0 = 128;
        overlay->tile.y0 = 0;
        if (level < 0) {
            blend = 1;
            level = -level;
        }
        SetDrawMode(&overlay->mode, 0, 0, GetTPage(0, blend, 0x3c0, 0x100), &g_gfx_texture_window);
        setRGB0(&overlay->tile, level, level, level);
        if (g_gfx_draw_primitives_immediately != 0) {
            gfx_draw_or_append_gpu_primitive(&overlay->mode);
            gfx_draw_or_append_gpu_primitive(&overlay->tile);
        } else {
            gfx_draw_or_append_gpu_primitive(&overlay->tile);
            gfx_draw_or_append_gpu_primitive(&overlay->mode);
        }
        g_script_event_input = 0;
    }
}
