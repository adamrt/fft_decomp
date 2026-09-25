#include "fft/event_require.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Double-buffered war-trophy primitives: one quad per entry, the banner
 * sprite and the draw mode; the second buffer is a copy of the first. */
typedef struct require_reward_war_trophy_primitives {
    POLY_FT4 quads[16]; /* 0x000 */
    SPRT banner;        /* 0x280 */
    DR_MODE draw_mode;  /* 0x294 */
} require_reward_war_trophy_primitives_t;

extern require_reward_war_trophy_primitives_t g_require_reward_war_trophy_primitives[2];

void require_render_war_trophies(void) {
    SPRT sprite;
    s32 start;
    s32 last;
    s32 row;
    s32 i;
    POLY_FT4* quad;
    require_reward_war_trophy_primitives_t* buffer;
    require_reward_war_trophy_primitives_t* prims;
    u32* image;
    s32 frame;
    s32 loaded;
    s32 timer;
    s32 thread;

    require_reward_collect_war_trophies_and_bonus_money();

    if (g_require_reward_war_trophy_entry_count == 0) {
        battle_thread_exit_current();
    }
    prims = g_require_reward_war_trophy_primitives;
    thread = battle_thread_resolve_id(0x10);
    last = 0;
    battle_thread_start(thread, require_render_display_condition_special_cases_thread);
    battle_thread_set_parameters(thread, 4, 0, 0);
    for (start = 0; start < g_require_reward_war_trophy_entry_count; start += 8) {
        row = 0;
        ClearImage(&g_require_reward_war_trophy_clear_rect, 0, 0, 0);
        battle_thread_yield();
        buffer = g_require_reward_war_trophy_primitives;
        battle_gfx_set_draw_mode_for_texture_page(&buffer->draw_mode, 1);
        for (i = 0; i < 16; i++) {
            battle_gfx_init_default_poly_ft4(&buffer->quads[i]);
        }
        battle_menu_init_semitransparent_sprt(&buffer->banner);
        prims->banner.x0 = 0x88;
        prims->banner.y0 = 0x78;
        prims->banner.u0 = 8;
        prims->banner.v0 = 0x78;
        prims->banner.h = 0x78;
        prims->banner.w = 0xf0;
        prims->banner.clut = 0x7cbc;
        battle_copy_bytes(
            &prims[1], g_require_reward_war_trophy_primitives, sizeof(require_reward_war_trophy_primitives_t));
        frame = 0;
        while (1) {
            buffer
                = (require_reward_war_trophy_primitives_t*)((frame & 1) * sizeof(require_reward_war_trophy_primitives_t)
                    + (s32)prims);
            if ((*g_require_input_controller & PSX_PAD_CIRCLE) || frame == 0
                || ((row != 7 && g_require_reward_war_trophy_entry_count - 1 != last) && timer >= 0x1f)
                || ((row == 7 || g_require_reward_war_trophy_entry_count - 1 == last) && timer >= 0x79)) {
                if (frame != 0) {
                    row++;
                    last++;
                }
                if (last == g_require_reward_war_trophy_entry_count || (row != 0 && (row & 7) == 0)) {
                    break;
                }
                image = battle_menu_build_and_upload_window_frame_image(
                    0x50, 0x10, &g_require_reward_war_trophy_image_rect, 1);
                battle_clear_menu_render_buffer(image, 0x280);
                g_menu_text_state.stride = 0x50;
                timer = 0;
                if (g_require_reward_war_trophy_entry_types[last] == 0) {
                    battle_menu_set_text_origin(0, 0);
                    battle_menu_display_text_entry(
                        g_require_reward_war_trophy_entry_values[last] + 0x3800, image, &g_menu_text_state.origin_x);
                } else {
                    battle_menu_set_text_origin(0, 0);
                    battle_draw_menu_number_glyphs(g_require_reward_war_trophy_entry_values[last],
                        battle_text_count_decimal_digits(g_require_reward_war_trophy_entry_values[last]), image,
                        (battle_rect_t*)&g_menu_text_state.origin_x);
                }
                if (g_require_reward_war_trophy_entry_count < 5) {
                    g_require_reward_war_trophy_image_rect.x = 0x60;
                    g_require_reward_war_trophy_image_rect.y = row * 20 + 0x78;
                } else {
                    g_require_reward_war_trophy_image_rect.x = (row & 1) ? 0x94 : 0x2c;
                    g_require_reward_war_trophy_image_rect.y = (row / 2) * 20 + 0x78;
                }
                g_require_reward_war_trophy_image_rect.x = g_require_reward_war_trophy_image_rect.x / 4 + 0x1c0;
                g_require_reward_war_trophy_image_rect.y = g_require_reward_war_trophy_image_rect.y;
                loaded = 1;
                LoadImage(&g_require_reward_war_trophy_image_rect, image);
            }
            battle_thread_yield();
            if (loaded) {
                battle_menu_free_memory(image);
                loaded = 0;
            }
            for (i = start; i <= last; i++) {
                quad = &buffer->quads[i];
                if (g_require_reward_war_trophy_entry_types[i] == 0) {
                    quad->x0 = require_reward_get_war_trophy_column_x(i, g_require_reward_war_trophy_entry_count);
                    quad->y0 = require_reward_get_war_trophy_entry_offset(
                        i - start, g_require_reward_war_trophy_entry_count);
                    quad->x1
                        = require_reward_get_war_trophy_column_x(i, g_require_reward_war_trophy_entry_count) + 0x10;
                    quad->y1 = require_reward_get_war_trophy_entry_offset(
                        i - start, g_require_reward_war_trophy_entry_count);
                    quad->x2 = require_reward_get_war_trophy_column_x(i, g_require_reward_war_trophy_entry_count);
                    quad->y2
                        = require_reward_get_war_trophy_entry_offset(i - start, g_require_reward_war_trophy_entry_count)
                        + 0x10;
                    quad->x3
                        = require_reward_get_war_trophy_column_x(i, g_require_reward_war_trophy_entry_count) + 0x10;
                    quad->y3
                        = require_reward_get_war_trophy_entry_offset(i - start, g_require_reward_war_trophy_entry_count)
                        + 0x10;
                    battle_get_item_graphic_data(&sprite, g_require_reward_war_trophy_entry_values[i]);
                    quad->u0 = sprite.u0;
                    quad->v0 = sprite.v0;
                    quad->u1 = sprite.u0 + 0x10;
                    quad->v1 = sprite.v0;
                    quad->u2 = sprite.u0;
                    quad->v2 = sprite.v0 + 0x10;
                    quad->u3 = sprite.u0 + 0x10;
                    quad->v3 = sprite.v0 + 0x10;
                    quad->clut = sprite.clut;
                    quad->tpage = GetTPage(0, 0, 0x380, 0x120);
                    battle_gfx_draw_or_append_gpu_primitive(quad);
                } else {
                    quad->x0 = require_reward_get_war_trophy_column_x(i, g_require_reward_war_trophy_entry_count)
                        + battle_text_count_decimal_digits(g_require_reward_war_trophy_entry_values[i]) * 7 + 0x12;
                    quad->y0
                        = require_reward_get_war_trophy_entry_offset(i - start, g_require_reward_war_trophy_entry_count)
                        + 4;
                    quad->x1 = require_reward_get_war_trophy_column_x(i, g_require_reward_war_trophy_entry_count)
                        + battle_text_count_decimal_digits(g_require_reward_war_trophy_entry_values[i]) * 7 + 0x24;
                    quad->y1
                        = require_reward_get_war_trophy_entry_offset(i - start, g_require_reward_war_trophy_entry_count)
                        + 4;
                    quad->x2 = require_reward_get_war_trophy_column_x(i, g_require_reward_war_trophy_entry_count)
                        + battle_text_count_decimal_digits(g_require_reward_war_trophy_entry_values[i]) * 7 + 0x12;
                    quad->y2
                        = require_reward_get_war_trophy_entry_offset(i - start, g_require_reward_war_trophy_entry_count)
                        + 0xc;
                    quad->x3 = require_reward_get_war_trophy_column_x(i, g_require_reward_war_trophy_entry_count)
                        + battle_text_count_decimal_digits(g_require_reward_war_trophy_entry_values[i]) * 7 + 0x24;
                    quad->y3
                        = require_reward_get_war_trophy_entry_offset(i - start, g_require_reward_war_trophy_entry_count)
                        + 0xc;
                    quad->u0 = 0x76;
                    quad->v0 = 0x10;
                    quad->u1 = 0x88;
                    quad->v1 = 0x10;
                    quad->u2 = 0x76;
                    quad->v2 = 0x18;
                    quad->u3 = 0x88;
                    quad->v3 = 0x18;
                    quad->clut = 0x7cbc;
                    quad->tpage = GetTPage(0, 0, 0x3c0, 0x100);
                    battle_gfx_draw_or_append_gpu_primitive(quad);
                }
            }
            battle_gfx_draw_or_append_gpu_primitive(&buffer->banner);
            battle_gfx_draw_or_append_gpu_primitive(&buffer->draw_mode);
            frame++;
            timer++;
        }
        if (loaded) {
            battle_menu_free_memory(image);
        }
    }
    battle_thread_set_parameters(thread, 0, 0, 1);
    battle_thread_wait_until_inactive(thread);
    battle_thread_exit_current();
}
