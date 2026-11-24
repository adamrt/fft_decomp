/* Graded menu-panel fades and the full-screen additive or subtractive fade overlay. */

#include "fft/gfx/fade.h"
#include "fft/gfx/gfx_image.h"
#include "fft/gfx/gfx_primitive.h"
#include "fft/menu/menu.h"
#include "fft/menu/menu_gfx.h"
#include "fft/script/script.h"
#include "fft/sys/memory.h"
#include "psx/libgpu.h"
#include "psx/types.h"

static menu_gradient_panel_prims_t s_menu_panel_fade_frames[2];

/* Build both buffers' panel fade primitives: a subtractive (abr 2) semi-transparent 256 x 50
 * center tile at (128, 172) with nine one-line tiles above and below it for the graded edges
 * that menu_update_panel_fade animates. The tiles start black, which subtracts nothing. */
void menu_init_panel_fade_tiles(void) {
    menu_gradient_panel_prims_t* frame = &s_menu_panel_fade_frames[0];
    s32 i;

    SetDrawMode(&frame->draw_mode, 0, 0, GetTPage(0, 2, 0x3c0, 0x100), &g_gfx_texture_window);

    SetTile(&frame->center);
    setRGB0(&frame->center, 0, 0, 0);
    SetSemiTrans(&frame->center, 1);
    frame->center.x0 = 128;
    frame->center.y0 = 172;
    frame->center.w = 256;
    frame->center.h = 50;

    for (i = 0; i < 9; i++) {
        SetTile(&frame->top[i]);
        setRGB0(&frame->top[i], 0, 0, 0);
        SetSemiTrans(&frame->top[i], 1);
        frame->top[i].w = 256;
        frame->top[i].h = 1;
        frame->top[i].x0 = 128;
        frame->top[i].y0 = i + 0xa3;

        SetTile(&frame->bottom[i]);
        setRGB0(&frame->bottom[i], 0, 0, 0);
        SetSemiTrans(&frame->bottom[i], 1);
        frame->bottom[i].w = 256;
        frame->bottom[i].h = 1;
        frame->bottom[i].x0 = 128;
        frame->bottom[i].y0 = i + 0xde;
    }

    mem_copy_bytes_nonempty(
        &s_menu_panel_fade_frames[1], &s_menu_panel_fade_frames[0], sizeof(menu_gradient_panel_prims_t));
}

/* Step the panel fade by one frame and submit this buffer's tiles. Fading in raises the
 * center's gray level by 8 per frame up to 80, fading out lowers it by 8 and then ends the
 * fade. The level is how much the tiles subtract from the scene, so the panel area darkens;
 * each edge row outward from the center is 8 lighter, giving a soft top and bottom edge. */
void menu_update_panel_fade(void) {
    menu_gradient_panel_prims_t* frame;
    s32 i;
    s32 intensity;

    if (g_menu_panel_fade_mode != MENU_PANEL_FADE_NONE) {
        frame = &s_menu_panel_fade_frames[g_menu_packet_buffer_index];
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
            setRGB0(&frame->top[8 - i], intensity, intensity, intensity);
            setRGB0(&frame->bottom[i], intensity, intensity, intensity);
            gfx_draw_or_append_gpu_primitive(&frame->top[8 - i]);
            gfx_draw_or_append_gpu_primitive(&frame->bottom[i]);
        }
        /* Indexed again rather than through `frame`, as the bytes require. */
        gfx_draw_or_append_gpu_primitive(&s_menu_panel_fade_frames[g_menu_packet_buffer_index].center);
        gfx_draw_or_append_gpu_primitive(&s_menu_panel_fade_frames[g_menu_packet_buffer_index].draw_mode);
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
void fade_event_submit_tile(fade_event_tile_t* overlay, s32 level) {
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
