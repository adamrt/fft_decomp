#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/battle_menu_window.h"
#include "fft/event.h"
#include "fft/main_gfx.h"
#include "fft/main_heap.h"
#include "fft/menu.h"
#include "fft/menu_types.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Provisional 0x2ec-byte double-buffered page of the unit status panel
 * (pairs at 0x8017225c, first page per mode from g_battle_unit_status_first_page_by_mode). */
typedef struct battle_unit_status_page {
    POLY_G4 bars[3];                         /* 0x000: HP/MP/CT gauges */
    DR_MODE draw_mode_1;                     /* 0x06c */
    DR_MODE draw_mode_0;                     /* 0x078 */
    world_menu_palette_primitives_t palette; /* 0x084 */
    POLY_FT4 quads[5];                       /* 0x170 */
    SPRT sprites[7];                         /* 0x238 */
    POLY_FT4 portrait;                       /* 0x2c4 */
} battle_unit_status_page_t;

/* Two DR_OFFSET packets, each followed by the x/y pair SetDrawOffset reads. */
typedef struct battle_unit_status_offsets {
    u32 draw_offset_a[3]; /* 0x00 */
    s16 offset_a[2];      /* 0x0c */
    u32 draw_offset_b[3]; /* 0x10 */
    s16 offset_b[2];      /* 0x1c */
} battle_unit_status_offsets_t;

extern void battle_world_display_specific_menu_text(s32 buffer, s32 position, s32 text);

extern battle_unit_status_page_t g_battle_unit_status_panel_pages[];
/* g_battle_menu_sprite_page_image_params[5..7], the unscaled templates of
 * elements 0..2; bound separately because the target addresses the two
 * arrays from different bases. */
extern world_gfx_image_load_parameters_t g_battle_menu_sprite_page_image_param_bases[];

/*
 * Unit status panel thread body (HP/MP/CT gauges, portrait, numbers and, in
 * mode 2, the reaction-ability name), the BATTLE twin of world_menu_display_hovered_unit_stats.
 * Builds two alternating 0x2ec-byte pages, slides the panel in by
 * `slide`/`speed` each event tick and out when the thread's third parameter
 * is set (mode 0 also leaves immediately while g_battle_thread_contexts[12].task_id is 0x13). A running
 * task-0x15 thread dims the portrait, gauges and CLUTs.
 *
 * `i` doubles as the saved hide-numbers flag in the first-frame text pass, as
 * in the target's register use. Gauge corners are stored in setXYWH order
 * (x0, y0, x1, y1, ...); storing y0/y1 first lengthens the reloaded
 * `extent_w`'s lifetime and swaps its register with the gauge width.

 */
void battle_menu_display_hovered_unit_stats(
    RECT* frame_rect, s32 mode, struct menu_number_entry* entries, battle_unit_status_record_t* status) {
    RECT rects[4];
    battle_unit_status_offsets_t offsets[2];
    u16 extent_w;
    u16 extent_h;
    u8* buffer;
    u8* text;
    s32 text_loaded;
    s32 running;
    s32 unit_id;
    s32 slide;
    s32 speed;
    s32 text_id;
    battle_stats_t* unit;
    battle_unit_status_page_t* page;
    world_gfx_image_load_parameters_t* params;
    battle_unit_status_offsets_t* offs;
    POLY_G4* poly;
    battle_unit_status_gauge_t* gauge;
    CVECTOR* color;
    s32* inner_width;
    s32 frame;
    s32 i;
    s32 hide_values;
    s32 base_y;
    s32 row_y;
    u16 width;

    unit_id = status->battle_id;
    speed = 0x16;
    unit = battle_unit_get_stats_from_battle_id(unit_id);
    slide = 0x84;
    g_battle_menu_hide_numeric_values = unit->unit_flags & 4;
    if (battle_thread_get_current_task_id() == 0x11) {
        g_battle_menu_hovered_stats_frame_count++;
    }
    if (mode == 2) {
        text_id = unit->reaction_ability;
        if (text_id >= 0x200) {
            text_id = 0;
        }
        if (text_id != 0) {
            text = battle_text_init_entry(text_id + 0x7000);
            g_battle_thread_call_target = (void (*)(void))battle_text_measure_pixels;
            battle_thread_call_on_main_stack(&extent_w, &extent_h, text);
            if (extent_w & 3) {
                extent_w = (extent_w & 0xFFFC) + 4;
            }
            extent_w = 0x4C - extent_w;
            if (extent_w > 0x100) {
                extent_w = 0;
            }
            g_battle_menu_sprite_page_image_params[25].x_screen_offset
                = g_battle_menu_sprite_page_image_params[28].x_screen_offset + extent_w;
            g_battle_menu_sprite_page_image_params[26].x_screen_offset
                = g_battle_menu_sprite_page_image_params[29].x_screen_offset + extent_w;
            for (frame = 0; frame < 3; frame++) {
                g_battle_menu_sprite_page_image_params[frame].x_screen_offset
                    = g_battle_menu_sprite_page_image_param_bases[frame].x_screen_offset + extent_w;
                g_battle_menu_sprite_page_image_params[frame].width
                    = g_battle_menu_sprite_page_image_param_bases[frame].width - extent_w;
            }
            g_battle_menu_sprite_page_image_params[3].x_screen_offset
                = g_battle_menu_sprite_page_image_params[8].x_screen_offset + extent_w;
        }
    }
    page = &g_battle_unit_status_panel_pages[g_battle_unit_status_first_page_by_mode[mode]];
    battle_menu_init_numeric_display_frame_primitives(frame_rect, &page->palette);
    battle_gfx_set_draw_mode_for_texture_page(&page->draw_mode_1, 1);
    battle_gfx_set_draw_mode_for_texture_page(&page->draw_mode_0, 0);
    battle_menu_init_sprite_array(page->sprites, 7, 0x7CBC);
    for (frame = 0; frame < 5; frame++) {
        battle_gfx_init_default_poly_ft4(&page->quads[frame]);
    }
    battle_gfx_init_default_poly_ft4(&page->portrait);
    battle_text_configure_sprite_vram(&rects[0], 0x28, 0x28, (SPRT*)&page->quads[0], 2);
    battle_text_configure_sprite_vram(&rects[1], 0x60, 0x10, (SPRT*)&page->quads[1], 2);
    params = g_battle_unit_status_quad_image_params_by_mode[mode];
    if (mode == 1) {
        battle_gfx_init_image_loading(
            &page->quads[4], &g_battle_menu_texture_location, (const battle_image_location_t*)frame_rect, &params[4]);
        for (frame = 0; frame < 2; frame++, params++) {
            battle_gfx_init_image_loading(&page->quads[frame], &g_battle_menu_texture_location,
                (const battle_image_location_t*)frame_rect, params);
            page->quads[frame].clut = 0x7CBC;
        }
        battle_menu_display_projected_action_effect(
            &page->quads[frame], &g_battle_menu_texture_location, (const battle_image_location_t*)frame_rect, params);
    } else if (mode == 2) {
        for (frame = 0; frame < 2; frame++, params++) {
            battle_gfx_init_image_loading(&page->quads[frame], &g_battle_menu_texture_location,
                (const battle_image_location_t*)frame_rect, params);
        }
        if (text_id != 0) {
            battle_gfx_init_image_loading(&page->quads[frame], &g_battle_menu_texture_location,
                (const battle_image_location_t*)frame_rect, params);
        }
    } else {
        for (frame = 0; frame < 4; frame++, params++) {
            battle_gfx_init_image_loading(&page->quads[frame], &g_battle_menu_texture_location,
                (const battle_image_location_t*)frame_rect, params);
        }
    }
    battle_copy_bytes(&page[1], page, sizeof(battle_unit_status_page_t));
    text_loaded = 0;
    for (frame = 0;; frame++) {
        if (status->gauges[2].value > 100) {
            status->gauges[2].value = 100;
        }
        if (text_loaded != 0) {
            battle_menu_free_memory(buffer);
            text_loaded = 0;
        }
        for (i = 0; i < g_battle_event_speed; i++) {
            if (battle_thread_get_current_parameter_3() != 0) {
                if (mode == 0 && g_battle_thread_contexts[12].task_id == 0x13) {
                    /* Both exits leave the step loop and the frame loop. */
                    goto exit;
                }
                slide += 0x1E;
                if (slide > 0x80) {
                    goto exit;
                }
            } else {
                if (slide != 0) {
                    slide -= speed;
                    if (slide < 0) {
                        slide = 0;
                    }
                }
                if (speed >= 3) {
                    speed -= 2;
                }
            }
        }
        running = battle_thread_find_running_by_task(NATIVE_THREAD_TASK_UNIT_EDITOR_PANEL);
        if (running != 0 && battle_thread_is_running_8014cc94(running - 1) == 0) {
            running = 0;
        }
        page = &g_battle_unit_status_panel_pages[g_battle_unit_status_first_page_by_mode[mode] + (frame & 1)];
        battle_menu_build_unit_portrait_poly(&page->portrait, unit_id);
        if (running != 0) {
            page->portrait.r0 = 0x40;
            page->portrait.g0 = 0x50;
            page->portrait.b0 = 0x70;
        } else {
            page->portrait.r0 = 0x80;
            page->portrait.g0 = 0x80;
            page->portrait.b0 = 0x80;
        }
        status->team_kind = 0;
        if (unit->initial_team_flags & 0x30) {
            status->team_kind = 1;
        }
        if (!(unit->initial_team_flags & 0x38)) {
            status->team_kind = 2;
        }
        if (unit->auto_battle_setting != 0) {
            status->team_kind = 3;
        }
        params = g_battle_unit_status_sprite_image_params_by_mode[mode];
        if (g_battle_unit_status_bounce_step == 0) {
            page->portrait.x0 = frame_rect->x + 2;
            page->portrait.y0 = frame_rect->y + 2;
            page->portrait.x1 = frame_rect->x + 0x21;
            page->portrait.y1 = frame_rect->y + 2;
            page->portrait.x2 = frame_rect->x + 2;
            page->portrait.y2 = frame_rect->y + 0x32;
            page->portrait.x3 = frame_rect->x + 0x21;
            page->portrait.y3 = frame_rect->y + 0x32;
            battle_copy_bytes(&params[6], &g_battle_unit_status_icon_image_params[status->team_kind], sizeof(*params));
            for (i = 0; i < 7; i++, params++) {
                battle_gfx_init_image_loading((POLY_FT4*)&page->sprites[i], &g_battle_menu_texture_location,
                    (const battle_image_location_t*)frame_rect, params);
            }
        }
        if (mode != 1) {
            for (i = 0; i < 4; i++) {
                page->quads[i].clut = g_battle_unit_status_quad_clut_pairs[i][running & 1];
            }
        }
        poly = page->bars;
        if (mode == 2) {
            page->quads[1].clut = 0x7C3C;
            page->quads[2].clut = 0x7D7C;
        }
        color = g_battle_unit_status_gauge_colors;
        gauge = status->gauges;
        for (i = 0; i < 3; i++) {
            SetPolyG4(poly);
            extent_h = 0xC2;
            extent_w = frame_rect->x + 0x2F;
            if (mode == 2 || mode == 3) {
                extent_w -= 0x6E;
                extent_h -= 0x10;
            }
            width = (gauge->value << 5) / gauge->max;
            if (width == 0) {
                extent_w = 0;
            }
            hide_values = g_battle_menu_hide_numeric_values;
            if (hide_values != 0) {
                width = 0x20;
            }
            row_y = extent_h + i * 0xB;
            poly->x0 = extent_w;
            poly->y0 = row_y;
            poly->x1 = extent_w + width;
            poly->y1 = row_y;
            poly->x2 = extent_w;
            poly->y2 = row_y + 3;
            poly->x3 = extent_w + width;
            poly->y3 = row_y + 3;
            width = (gauge->value2 << 5) / gauge->max;
            if (width == 0) {
                extent_w = 0;
            }
            poly->x2 -= 3;
            poly->x3 -= 3;
            if (hide_values != 0) {
                poly->r0 = 0x40;
                poly->g0 = 0x40;
                poly->b0 = 0x40;
                poly->r1 = 0x80;
                poly->g1 = 0x80;
                poly->b1 = 0x80;
                poly->r2 = 0x40;
                poly->g2 = 0x40;
                poly->b2 = 0x40;
                poly->r3 = 0xA0;
                poly->g3 = 0xC4;
                poly->b3 = 0xC4;
            } else if (running != 0) {
                poly->r0 = color->r >> 1;
                poly->g0 = color->g >> 1;
                poly->b0 = color->b;
                color++;
                poly->r1 = color->r >> 1;
                poly->g1 = color->g >> 1;
                poly->b1 = color->b;
                color++;
                poly->r2 = color->r >> 1;
                poly->g2 = color->g >> 1;
                poly->b2 = color->b;
                color++;
                poly->r3 = color->r >> 1;
                poly->g3 = color->g >> 1;
                poly->b3 = color->b;
                color++;
            } else {
                poly->r0 = color->r;
                poly->g0 = color->g;
                poly->b0 = color->b;
                color++;
                poly->r1 = color->r;
                poly->g1 = color->g;
                poly->b1 = color->b;
                color++;
                poly->r2 = color->r;
                poly->g2 = color->g;
                poly->b2 = color->b;
                color++;
                poly->r3 = color->r;
                poly->g3 = color->g;
                poly->b3 = color->b;
                color++;
            }
            poly++;
            gauge++;
        }
        if (frame == 0) {
            buffer = battle_menu_alloc_buffer(0x700);
            battle_clear_menu_render_buffer(buffer, 0x700);
            inner_width = &g_menu_inner_window_width;
            *inner_width = 0x28;
            battle_menu_draw_numeric_display_entries(
                (s32)buffer, entries, (struct menu_number_position*)(inner_width - 2), 6);
            LoadImage(&rects[0], (u32*)buffer);
            i = g_battle_menu_hide_numeric_values;
            *inner_width = 0x60;
            g_battle_menu_hide_numeric_values = 0;
            if (mode == 0 || mode == 3) {
                battle_menu_draw_numeric_display_entries(
                    (s32)(buffer + 0x400), entries + 6, (struct menu_number_position*)(inner_width - 2), 2);
                g_battle_menu_hide_numeric_values = i;
                battle_menu_draw_numeric_display_entries(
                    (s32)(buffer + 0x400), entries + 8, (struct menu_number_position*)(inner_width - 2), 2);
            } else if (mode == 1) {
                battle_menu_draw_numeric_display_entries(
                    (s32)(buffer + 0x400), entries + 6, (struct menu_number_position*)(inner_width - 2), 2);
                g_battle_menu_hide_numeric_values = i;
            } else if (text_id != 0) {
                battle_menu_set_text_origin(2, 0);
                battle_world_display_specific_menu_text((s32)(buffer + 0x400), (s32)(inner_width - 2), (s32)text);
            }
            LoadImage(&rects[1], (u32*)(buffer + 0x400));
            text_loaded = 1;
        }
        offs = &offsets[frame & 1];
        base_y = (g_main_gfx_screen_polarity & 1) == 0 ? 0xF0 : 0;
        if (mode == 2 || mode == 3) {
            offs->offset_a[0] = slide - 0x80;
        } else {
            offs->offset_a[0] = -0x80 - slide;
        }
        if (g_battle_unit_status_bounce_step != 0) {
            offs->offset_a[1] = base_y + g_battle_unit_status_bounce_offsets[g_battle_unit_status_bounce_step];
        } else {
            offs->offset_a[1] = base_y;
        }
        SetDrawOffset(offs->draw_offset_a, offs->offset_a);
        offs->offset_b[0] = -0x80;
        offs->offset_b[1] = base_y;
        SetDrawOffset(offs->draw_offset_b, offs->offset_b);
        battle_gfx_draw_or_append_gpu_primitive((s32*)offs->draw_offset_b);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->quads[0]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->quads[1]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->draw_mode_1);
        poly--;
        for (i = 0; i < 3; i++) {
            battle_gfx_draw_or_append_gpu_primitive((s32*)poly);
            poly--;
        }
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->quads[2]);
        if (mode == 1) {
            page->quads[4].clut = 0x7D7C;
            battle_gfx_draw_or_append_gpu_primitive((s32*)&page->quads[4]);
        }
        if (mode != 2) {
            battle_gfx_draw_or_append_gpu_primitive((s32*)&page->quads[3]);
        } else if (text_id != 0) {
            battle_menu_build_sprite_page(frame, frame_rect);
        }
        for (i = 0; i < 7; i++) {
            page->sprites[i].clut = g_battle_unit_status_sprite_clut_pairs[i][running & 1];
            battle_gfx_draw_or_append_gpu_primitive((s32*)&page->sprites[i]);
        }
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->draw_mode_0);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&page->portrait);
        if (running != 0) {
            battle_menu_init_primitive_colors_palette_bank_1(&page->palette);
        } else {
            battle_menu_init_primitive_colors_palette_bank_0(&page->palette);
        }
        battle_menu_submit_numeric_display_frame_primitives((u8*)&page->palette);
        battle_gfx_draw_or_append_gpu_primitive((s32*)offs->draw_offset_a);
        battle_thread_yield();
        g_battle_menu_hide_numeric_values = unit->unit_flags & 4;
        if (battle_thread_get_current_task_id() == 0x11) {
            g_battle_menu_hovered_stats_frame_count++;
        }
    }
exit:
    battle_thread_yield();
    if (battle_thread_get_current_task_id() == 0x11) {
        g_battle_menu_hovered_stats_frame_count = 0;
    }
    battle_gfx_free_tpage7_vram(&rects[0]);
    battle_gfx_free_tpage7_vram(&rects[1]);
    battle_thread_exit_current();
}
