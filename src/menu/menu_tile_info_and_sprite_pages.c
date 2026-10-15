/* Height display flags, text sprite VRAM, selected tile info thread and sprite page building. */

#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_text.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

void menu_suppress_height_display(void) {
    g_menu_height_display_suppressed = 1;
}

void menu_enable_height_display(void) {
    g_menu_height_display_suppressed = 0;
}

/* Allocate text image VRAM for the sprite or textured quad and give it the normal menu CLUT, GetClut(0x3c0, 0x1f0),
 * plus clut_offset sixteens of the 16-bit CLUT value (4 is one VRAM line down). Callers pass 0 or 2; 2 carries out
 * of the x field: 16-bit CLUT value 0x7c5c is GetClut(0x1c0, 0x1f1). */
void text_configure_sprite_vram(RECT* rect, s32 width, s32 height, void* packet, s32 clut_offset) {
    texture_primitive_t* primitive = packet;

    rect->w = width;
    rect->h = height;
    gfx_alloc_texture_rect(rect, primitive, (u32*)-1);
    primitive->sprt.clut = (clut_offset << 4) + 0x7c3c;
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * text_configure_sprite_vram and menu_run_selected_tile_info_display_thread.
 * Nothing on the disc calls it or points at it; the name is a neutral
 * placeholder until a reference turns up. */
void menu_noop_1(void) {
}

/* Draw the selected tile height and depth while its panel thread is active. */
void menu_run_selected_tile_info_display_thread(void) {
    RECT rect;
    u8 buffer[0x100];
    s32 i;
    s32 j;
    s32 frame;
    menu_quad_page_t* page;
    POLY_GT4* quad;

    g_menu_hide_numeric_values = 0;
    page = &g_menu_height_display_pages[0];
    gfx_set_draw_mode_for_texture_page(&page->modes[0], MENU_TEXTURE_PAGE_ALLOCATED_IMAGES);
    gfx_set_draw_mode_for_texture_page(&page->modes[1], MENU_TEXTURE_PAGE_FRAME);
    for (i = 0; i < 5; i++) {
        quad = &page->quads[i];
        SetPolyGT4(quad);
        SetShadeTex(quad, 0);
        SetSemiTrans(quad, 1);
        quad->tpage = GetTPage(0, 0, 0x380, 0x120);
        quad->clut = 0x7CFC;
    }
    menu_init_sprite_array(page->sprites, 2, 0x7CBC);
    page->sprites[1].clut = 0x7D7C;
    for (i = 0; i < 2; i++) {
        gfx_init_image_loading(&page->sprites[i], &g_menu_texture_origin, &g_menu_tile_info_image_location,
            &g_menu_tile_info_image_params[i]);
    }
    text_configure_sprite_vram(&rect, 0x18, 0x10, &page->sprites[0], 0);
    mem_copy_bytes_nonempty(&g_menu_height_display_pages[1], &g_menu_height_display_pages[0], sizeof(menu_quad_page_t));

    for (frame = 0;; frame++) {
        page = &g_menu_height_display_pages[frame & 1];
        /* The target loads the display height signed (lh). */
        i = menu_build_quad_pieces(g_map_selected_tile_data.display_height, page->quads) - 1;
        thread_yield();
        if (thread_find_running_by_task(THREAD_TASK_DARK_SCREEN) != 0) {
            continue;
        }
        if (thread_find_running_by_task(THREAD_TASK_DARK_SCREEN_HOLD) != 0) {
            continue;
        }
        g_menu_hide_numeric_values = 0;
        if (thread_get_current_parameter_3_variant() != 0) {
            break;
        }
        for (j = i; j >= 0; j--) {
            gfx_draw_or_append_gpu_primitive(&page->quads[j]);
        }
        if (g_map_selected_tile_data.depth != 0) {
            menu_clear_render_buffer(buffer, 0x100);
            g_menu_text_state.pen.stride = 0x18;
            menu_set_text_origin(0x10, 0);
            menu_draw_number_glyphs(g_map_selected_tile_data.depth, 1, buffer, &g_menu_text_state.pen);
            if (g_menu_height_display_suppressed == 0) {
                LoadImage(&rect, buffer);
                page->sprites[0].clut = 0x7CBC;
                gfx_draw_or_append_gpu_primitive(&page->sprites[0]);
                gfx_draw_or_append_gpu_primitive(&page->modes[0]);
                gfx_draw_or_append_gpu_primitive(&page->sprites[1]);
                gfx_draw_or_append_gpu_primitive(&page->modes[1]);
            }
        }
    }
    thread_yield();
    gfx_free_texture_rect(&rect);
    thread_exit_current();
}

s32 menu_build_quad_pieces(s32 group, POLY_GT4* poly) {
    menu_quad_piece_t* piece;
    s32 count;
    s32 i;
    menu_quad_colors_t* colors;

    colors = &g_menu_quad_colors;
    piece = &g_menu_quad_pieces[g_menu_quad_group_starts[group]];
    count = g_menu_quad_group_starts[group + 1] - g_menu_quad_group_starts[group];
    for (i = 0; i < count; i++) {
        setXY4(poly, g_menu_quad_origin.x + piece->x, g_menu_quad_origin.y + piece->y,
            g_menu_quad_origin.x + piece->x + piece->w, g_menu_quad_origin.y + piece->y,
            g_menu_quad_origin.x + piece->x, g_menu_quad_origin.y + piece->y + (piece->h - 1),
            g_menu_quad_origin.x + piece->x + piece->w, g_menu_quad_origin.y + piece->y + (piece->h - 1));
        setUV4(poly, piece->u, piece->v - 0x78, piece->u + piece->w, piece->v - 0x78, piece->u,
            piece->v - 0x78 + (piece->h - 1), piece->u + piece->w, piece->v - 0x78 + (piece->h - 1));
        setRGB0(poly, colors->vertex[0].r, colors->vertex[0].g, colors->vertex[0].b);
        setRGB1(poly, colors->vertex[1].r, colors->vertex[1].g, colors->vertex[1].b);
        setRGB2(poly, colors->vertex[2].r, colors->vertex[2].g, colors->vertex[2].b);
        setRGB3(poly, colors->vertex[3].r, colors->vertex[3].g, colors->vertex[3].b);
        poly++;
        piece++;
    }
    return count;
}

/*
 * Build one of the two double-buffered menu sprite pages: five draw-mode
 * packets (texture window per sprite) followed by five sprites placed from
 * the parameter table, then submit them all in reverse order.
 *
 * BATTLE twin of menu_build_sprite_page.
 */
void menu_build_sprite_page(s32 page_index, RECT* base_screen) {
    s32 i;
    menu_sprite_page_t* page = &g_menu_scratch_buffer.sprite_pages[page_index & 1];
    SPRT* sprite;

    for (i = 0; i < 5; i++) {
        sprite = &page->sprites[i];
        menu_init_sprt_variant(sprite);
        SetDrawMode(&page->modes[i], 1, 0, GetTPage(0, 2, 0x3c0, 0x100), &g_menu_sprite_page_texture_windows[i]);
        gfx_init_image_loading(sprite, &g_menu_texture_origin, base_screen, &g_menu_sprite_page_image_params[i]);
    }
    for (i = 4; i >= 0; i--) {
        gfx_draw_or_append_gpu_primitive(&page->sprites[i]);
        gfx_draw_or_append_gpu_primitive(&page->modes[i]);
    }
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between menu_build_sprite_page
 * and menu_display_projected_action_effect_variant. Nothing on the disc calls it or
 * points at it; the name is a neutral placeholder until a reference turns up. */
void menu_noop_2(void) {
}
