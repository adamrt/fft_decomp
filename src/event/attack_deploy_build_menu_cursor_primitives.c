#include "fft/event_attack.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/libgpu.h"

/* Build and submit the deployment menu's animated cursor primitives. */
void attack_deploy_build_menu_cursor_primitives(s32 frame, u32 mode, attack_deploy_render_buffer_t* render_buffer) {
    s32 i;
    s32 palette;
    POLY_FT4* ptr;

    gfx_set_draw_mode_for_texture_page(&render_buffer->menu_draw_mode, MENU_TEXTURE_PAGE_FRAME);

    for (i = 0; i < 4; i++) {
        ptr = &render_buffer->menu_cursor[i];
        menu_init_default_poly_ft4(ptr);
        SetSemiTrans(ptr, 0);
        SetShadeTex(ptr, 1);
        /* The draw area's x/y serve as the screen base point. */
        gfx_init_image_loading(
            ptr, g_attack_deploy_texture_location, &g_menu_draw_area_template, &g_attack_deploy_menu_cursor_cells[i]);
    }

    if (mode != ATTACK_DEPLOY_MENU_BLOCKED_TILE) {
        palette = 0x7D7C;
        for (i = 3; i >= 0; i--) {
            render_buffer->menu_cursor[i].clut = palette;
        }
    } else {
        palette = 0x7DFC;
        for (i = 3; i >= 0; i--) {
            render_buffer->menu_cursor[i].clut = palette;
        }
    }

    gfx_draw_or_append_gpu_primitive(&render_buffer->menu_draw_mode);
    if (mode < 2) {
        gfx_draw_or_append_gpu_primitive(&render_buffer->menu_cursor[0]);
    } else if (mode == ATTACK_DEPLOY_MENU_SELECTED_UNIT) {
        gfx_draw_or_append_gpu_primitive(&render_buffer->menu_cursor[1]);
    } else if (mode == ATTACK_DEPLOY_MENU_OTHER_UNIT) {
        gfx_draw_or_append_gpu_primitive(&render_buffer->menu_cursor[1]);
        gfx_draw_or_append_gpu_primitive(&render_buffer->menu_cursor[0]);
        if ((frame & 0x3F) < 0x20) {
            render_buffer->menu_cursor[0].clut = 0x7DFC;
        } else {
            render_buffer->menu_cursor[1].clut = 0x7DFC;
        }
    } else if (mode == ATTACK_DEPLOY_MENU_SWAP_UNIT) {
        gfx_draw_or_append_gpu_primitive(&render_buffer->menu_cursor[3]);
        gfx_draw_or_append_gpu_primitive(&render_buffer->menu_cursor[2]);
        if ((frame & 0x3F) < 0x20) {
            render_buffer->menu_cursor[2].clut = 0x7DFC;
        } else {
            render_buffer->menu_cursor[3].clut = 0x7DFC;
        }
    }
}
