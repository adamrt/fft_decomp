#include "fft/event_require.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Thread body of the post-battle "war trophies" list.
 *
 * Collects the entries (require_reward_collect_war_trophies_and_bonus_money)
 * and exits at once when there are none. Otherwise it starts the banner
 * thread and reveals the entries eight to a page: a new entry appears every
 * 0x1f frames or on Circle, and the last entry of a page stays for 0x79
 * frames before the page is cleared. Fewer than five entries form one column,
 * more form two. An item entry is its 16x16 icon and name; any other entry is
 * a number followed by an 18x8 label glyph. Each new line of text is uploaded to
 * VRAM and its buffer freed on the following frame.
 */
void require_reward_run_war_trophies(void) {
    SPRT sprite;
    s32 start;
    s32 last;
    s32 row;
    s32 i;
    POLY_FT4* quad;
    require_reward_war_trophy_primitives_t* buffer;
    require_reward_war_trophy_primitives_t* prims;
    u8* image;
    s32 frame;
    s32 loaded;
    s32 timer;
    s32 thread;

    require_reward_collect_war_trophies_and_bonus_money();

    if (g_require_reward_war_trophy_entry_count == 0) {
        thread_exit_current();
    }
    prims = g_require_reward_war_trophy_primitives;
    thread = battle_thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
    last = 0;
    thread_start(thread, require_condition_run_special_thread);
    thread_set_parameters(thread, 4, 0, 0);
    for (start = 0; start < g_require_reward_war_trophy_entry_count; start += 8) {
        row = 0;
        ClearImage(&g_require_reward_war_trophy_clear_rect, 0, 0, 0);
        thread_yield();
        buffer = g_require_reward_war_trophy_primitives;
        battle_gfx_set_draw_mode_for_texture_page(&buffer->draw_mode, MENU_TEXTURE_PAGE_ALLOCATED_IMAGES);
        for (i = 0; i < 16; i++) {
            menu_init_default_poly_ft4(&buffer->quads[i]);
        }
        battle_menu_init_semitransparent_sprt(&buffer->banner);
        prims->banner.x0 = 0x88;
        prims->banner.y0 = 0x78;
        prims->banner.u0 = 8;
        prims->banner.v0 = 0x78;
        prims->banner.h = 0x78;
        prims->banner.w = 0xf0;
        prims->banner.clut = 0x7cbc;
        mem_copy_bytes_nonempty(
            &prims[1], g_require_reward_war_trophy_primitives, sizeof(require_reward_war_trophy_primitives_t));
        frame = 0;
        while (1) {
            /* Typed buffer indexing swaps the ADDU operands. */
            buffer
                = (require_reward_war_trophy_primitives_t*)((frame & 1) * sizeof(require_reward_war_trophy_primitives_t)
                    + (s32)prims);
            if ((*g_menu_input_controller & PSX_PAD_CIRCLE) || frame == 0
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
                    0x50, 0x10, &g_require_reward_war_trophy_image_rect, MENU_WINDOW_FRAME_BUILD);
                battle_clear_menu_render_buffer(image, 0x280);
                g_menu_text_state.pen.stride = 0x50;
                timer = 0;
                if (g_require_reward_war_trophy_entry_types[last] == REQUIRE_WAR_TROPHY_ENTRY_ITEM) {
                    battle_menu_set_text_origin(0, 0);
                    battle_menu_display_text_entry(
                        g_require_reward_war_trophy_entry_values[last] + TEXT_ID_ITEM_NAME_BASE, image,
                        &g_menu_text_state.pen);
                } else {
                    battle_menu_set_text_origin(0, 0);
                    battle_menu_draw_number_glyphs(g_require_reward_war_trophy_entry_values[last],
                        text_count_decimal_digits(g_require_reward_war_trophy_entry_values[last]), image,
                        &g_menu_text_state.pen);
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
                LoadImage(&g_require_reward_war_trophy_image_rect, (u32*)image);
            }
            thread_yield();
            if (loaded) {
                battle_menu_free_memory(image);
                loaded = 0;
            }
            for (i = start; i <= last; i++) {
                quad = &buffer->quads[i];
                if (g_require_reward_war_trophy_entry_types[i] == REQUIRE_WAR_TROPHY_ENTRY_ITEM) {
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
                    gfx_set_item_icon_sprite(&sprite, g_require_reward_war_trophy_entry_values[i]);
                    setUV4(quad, sprite.u0, sprite.v0, sprite.u0 + 0x10, sprite.v0, sprite.u0, sprite.v0 + 0x10,
                        sprite.u0 + 0x10, sprite.v0 + 0x10);
                    quad->clut = sprite.clut;
                    quad->tpage = GetTPage(0, 0, 0x380, 0x120);
                    gfx_draw_or_append_gpu_primitive(quad);
                } else {
                    quad->x0 = require_reward_get_war_trophy_column_x(i, g_require_reward_war_trophy_entry_count)
                        + text_count_decimal_digits(g_require_reward_war_trophy_entry_values[i]) * 7 + 0x12;
                    quad->y0
                        = require_reward_get_war_trophy_entry_offset(i - start, g_require_reward_war_trophy_entry_count)
                        + 4;
                    quad->x1 = require_reward_get_war_trophy_column_x(i, g_require_reward_war_trophy_entry_count)
                        + text_count_decimal_digits(g_require_reward_war_trophy_entry_values[i]) * 7 + 0x24;
                    quad->y1
                        = require_reward_get_war_trophy_entry_offset(i - start, g_require_reward_war_trophy_entry_count)
                        + 4;
                    quad->x2 = require_reward_get_war_trophy_column_x(i, g_require_reward_war_trophy_entry_count)
                        + text_count_decimal_digits(g_require_reward_war_trophy_entry_values[i]) * 7 + 0x12;
                    quad->y2
                        = require_reward_get_war_trophy_entry_offset(i - start, g_require_reward_war_trophy_entry_count)
                        + 0xc;
                    quad->x3 = require_reward_get_war_trophy_column_x(i, g_require_reward_war_trophy_entry_count)
                        + text_count_decimal_digits(g_require_reward_war_trophy_entry_values[i]) * 7 + 0x24;
                    quad->y3
                        = require_reward_get_war_trophy_entry_offset(i - start, g_require_reward_war_trophy_entry_count)
                        + 0xc;
                    setUV4(quad, 0x76, 0x10, 0x88, 0x10, 0x76, 0x18, 0x88, 0x18);
                    quad->clut = 0x7cbc;
                    quad->tpage = GetTPage(0, 0, 0x3c0, 0x100);
                    gfx_draw_or_append_gpu_primitive(quad);
                }
            }
            gfx_draw_or_append_gpu_primitive(&buffer->banner);
            gfx_draw_or_append_gpu_primitive(&buffer->draw_mode);
            frame++;
            timer++;
        }
        if (loaded) {
            battle_menu_free_memory(image);
        }
    }
    thread_set_parameters(thread, 0, 0, 1);
    thread_wait_until_inactive(thread);
    thread_exit_current();
}
