#include "fft/attack.h"
#include "fft/battle_gfx.h"
#include "fft/menu_types.h"
#include "psx/gpu.h"

void battle_gfx_init_image_loading(POLY_FT4* primitive, const battle_image_location_t* base_load,
    const battle_image_location_t* base_screen, const world_gfx_image_load_parameters_t* parameters);

/* Build and submit the deployment menu's animated cursor primitives.
 *
 * The 0xA2C-byte render-buffer layout is not established yet, so its proven
 * primitive offsets remain explicit until the surrounding owner is typed. */
void attack_deploy_build_menu_cursor_primitives(s32 frame, u32 mode, u8* render_buffer) {
    s32 i;
    s32 palette;
    u8* ptr;
    u8* p2;

    battle_gfx_set_draw_mode_for_texture_page(render_buffer + 0xA00, 0);

    i = 0;
    do {
        ptr = render_buffer + 0x884 + (i * 0x28);
        battle_gfx_init_default_poly_ft4((POLY_FT4*)ptr);
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
        p2 = render_buffer + 0x78;
        do {
            *(s16*)(p2 + 0x892) = palette;
            i -= 1;
            p2 -= 0x28;
        } while (i >= 0);
    } else {
        palette = 0x7DFC;
        i = 3;
        p2 = render_buffer + 0x78;
        do {
            *(s16*)(p2 + 0x892) = palette;
            i -= 1;
            p2 -= 0x28;
        } while (i >= 0);
    }

    battle_gfx_draw_or_append_gpu_primitive((s32*)(render_buffer + 0xA00));
    if (mode < 2) {
        battle_gfx_draw_or_append_gpu_primitive((s32*)(render_buffer + 0x884));
    } else if (mode == 2) {
        battle_gfx_draw_or_append_gpu_primitive((s32*)(render_buffer + 0x8AC));
    } else if (mode == 3) {
        battle_gfx_draw_or_append_gpu_primitive((s32*)(render_buffer + 0x8AC));
        battle_gfx_draw_or_append_gpu_primitive((s32*)(render_buffer + 0x884));
        if ((frame & 0x3F) < 0x20) {
            *(s16*)(render_buffer + 0x892) = 0x7DFC;
        } else {
            *(s16*)(render_buffer + 0x8BA) = 0x7DFC;
        }
    } else if (mode == 4) {
        battle_gfx_draw_or_append_gpu_primitive((s32*)(render_buffer + 0x8FC));
        battle_gfx_draw_or_append_gpu_primitive((s32*)(render_buffer + 0x8D4));
        if ((frame & 0x3F) < 0x20) {
            *(s16*)(render_buffer + 0x8E2) = 0x7DFC;
        } else {
            *(s16*)(render_buffer + 0x90A) = 0x7DFC;
        }
    }
}
