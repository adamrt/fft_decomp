#include "fft/event_attack.h"
#include "psx/gpu.h"

/* Build and submit the deployment menu's animated cursor primitives. */
void attack_deploy_build_menu_cursor_primitives(s32 frame, u32 mode, attack_deploy_render_buffer_t* render_buffer) {
    s32 i;
    s32 palette;
    POLY_FT4* ptr;
    u8* p2;

    battle_gfx_set_draw_mode_for_texture_page(&render_buffer->menu_draw_mode, 0);

    i = 0;
    do {
        ptr = &render_buffer->menu_cursor[i];
        battle_gfx_init_default_poly_ft4(ptr);
        SetSemiTrans(ptr, 0);
        SetShadeTex(ptr, 1);
        /* The draw area's x/y serve as the screen base point. */
        battle_gfx_init_image_loading(ptr, g_attack_deploy_texture_location,
            (const battle_image_location_t*)&g_attack_gfx_draw_area_template,
            g_attack_deploy_menu_cursor_cells + (i * 0xC));
        i += 1;
    } while (i < 4);

    if (mode != 1) {
        palette = 0x7D7C;
        i = 3;
        /* The 0x78 cursor base yields the target's large store displacement. */
        p2 = (u8*)render_buffer + 0x78;
        do {
            *(s16*)(p2 + 0x892) = palette;
            i -= 1;
            p2 -= sizeof(POLY_FT4);
        } while (i >= 0);
    } else {
        palette = 0x7DFC;
        i = 3;
        p2 = (u8*)render_buffer + 0x78;
        do {
            *(s16*)(p2 + 0x892) = palette;
            i -= 1;
            p2 -= sizeof(POLY_FT4);
        } while (i >= 0);
    }

    battle_gfx_draw_or_append_gpu_primitive(&render_buffer->menu_draw_mode);
    if (mode < 2) {
        battle_gfx_draw_or_append_gpu_primitive(&render_buffer->menu_cursor[0]);
    } else if (mode == 2) {
        battle_gfx_draw_or_append_gpu_primitive(&render_buffer->menu_cursor[1]);
    } else if (mode == 3) {
        battle_gfx_draw_or_append_gpu_primitive(&render_buffer->menu_cursor[1]);
        battle_gfx_draw_or_append_gpu_primitive(&render_buffer->menu_cursor[0]);
        if ((frame & 0x3F) < 0x20) {
            render_buffer->menu_cursor[0].clut = 0x7DFC;
        } else {
            render_buffer->menu_cursor[1].clut = 0x7DFC;
        }
    } else if (mode == 4) {
        battle_gfx_draw_or_append_gpu_primitive(&render_buffer->menu_cursor[3]);
        battle_gfx_draw_or_append_gpu_primitive(&render_buffer->menu_cursor[2]);
        if ((frame & 0x3F) < 0x20) {
            render_buffer->menu_cursor[2].clut = 0x7DFC;
        } else {
            render_buffer->menu_cursor[3].clut = 0x7DFC;
        }
    }
}
