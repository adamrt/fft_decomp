#include "fft/battle_gfx.h"
#include "fft/battle_menu.h"
#include "fft/battle_text.h"
#include "fft/battle_unit.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Draw the unit gauges, portrait and optional reaction label until stopped. */
void menu_display_hovered_unit_stats(
    RECT* frame_rect, s32 mode, struct menu_number_entry* entries, menu_unit_status_record_t* status) {
    RECT rects[4];
    menu_unit_status_offsets_t offsets[2];
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
    unit_stats_t* unit;
    menu_unit_status_page_t* page;
    image_load_parameters_t* params;
    menu_unit_status_offsets_t* offs;
    POLY_G4* poly;
    menu_unit_status_gauge_t* gauge;
    CVECTOR* color;
    glyph_blit_t* origin;
    s32* stride;
    s32 frame;
    s32 i;
    s32 hide_values;
    s32 base_y;
    s32 row_y;
    u16 width;

    unit_id = status->unit_index;
    speed = 0x16;
    unit = battle_unit_get_stats_from_battle_id(unit_id);
    slide = 0x84;
    g_menu_hide_numeric_values = unit->unit_flags & UNIT_FLAG_EGG;
    if (thread_get_current_task_id() == THREAD_TASK_HOVERED_UNIT_STATS) {
        g_battle_menu_hovered_stats_frame_count++;
    }
    if (mode == UNIT_STATS_PANEL_ATTACK_TARGET) {
        text_id = unit->abilities.reaction;
        if (text_id >= ABILITY_ID_COUNT) {
            text_id = 0;
        }
        if (text_id != 0) {
            text = text_find_entry(text_id + TEXT_ID_ABILITY_NAME_BASE);
            g_thread_call_target = (void (*)(void))text_measure_pixels;
            thread_call_on_main_stack(&extent_w, &extent_h, text);
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
    battle_gfx_set_draw_mode_for_texture_page(&page->draw_mode_1, MENU_TEXTURE_PAGE_ALLOCATED_IMAGES);
    battle_gfx_set_draw_mode_for_texture_page(&page->draw_mode_0, MENU_TEXTURE_PAGE_FRAME);
    menu_init_sprite_array(page->sprites, 7, 0x7CBC);
    for (frame = 0; frame < 5; frame++) {
        menu_init_default_poly_ft4(&page->quads[frame]);
    }
    menu_init_default_poly_ft4(&page->portrait);
    text_configure_sprite_vram(&rects[0], 0x28, 0x28, &page->quads[0], 2);
    text_configure_sprite_vram(&rects[1], 0x60, 0x10, &page->quads[1], 2);
    params = g_battle_unit_status_quad_image_params_by_mode[mode];
    if (mode == UNIT_STATS_PANEL_ATTACK_CASTER) {
        gfx_init_image_loading(&page->quads[4], &g_menu_texture_origin, frame_rect, &params[4]);
        for (frame = 0; frame < 2; frame++, params++) {
            gfx_init_image_loading(&page->quads[frame], &g_menu_texture_origin, frame_rect, params);
            page->quads[frame].clut = 0x7CBC;
        }
        battle_menu_display_projected_action_effect(&page->quads[frame], &g_menu_texture_origin, frame_rect, params);
    } else if (mode == UNIT_STATS_PANEL_ATTACK_TARGET) {
        for (frame = 0; frame < 2; frame++, params++) {
            gfx_init_image_loading(&page->quads[frame], &g_menu_texture_origin, frame_rect, params);
        }
        if (text_id != 0) {
            gfx_init_image_loading(&page->quads[frame], &g_menu_texture_origin, frame_rect, params);
        }
    } else {
        for (frame = 0; frame < 4; frame++, params++) {
            gfx_init_image_loading(&page->quads[frame], &g_menu_texture_origin, frame_rect, params);
        }
    }
    mem_copy_bytes_nonempty(&page[1], page, sizeof(menu_unit_status_page_t));
    text_loaded = 0;
    for (frame = 0;; frame++) {
        if (status->gauges[MENU_UNIT_STATUS_GAUGE_CT].value > UNIT_TURN_CT_THRESHOLD) {
            status->gauges[MENU_UNIT_STATUS_GAUGE_CT].value = UNIT_TURN_CT_THRESHOLD;
        }
        if (text_loaded != 0) {
            menu_free_memory(buffer);
            text_loaded = 0;
        }
        for (i = 0; i < g_script_event_speed; i++) {
            if (battle_thread_get_current_parameter_3() != 0) {
                if (mode == UNIT_STATS_PANEL_HOVERED_UNIT
                    && g_thread_contexts[THREAD_SLOT_RIGHT_UNIT_STATS].task_id == THREAD_TASK_ATTACK_CASTER_STATS) {
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
        running = battle_thread_find_running_by_task(THREAD_TASK_UNIT_EDITOR_PANEL);
        if (running != 0 && battle_thread_is_running_asm(running - 1) == 0) {
            running = 0;
        }
        page = &g_battle_unit_status_panel_pages[g_battle_unit_status_first_page_by_mode[mode] + (frame & 1)];
        battle_menu_build_unit_portrait_poly(&page->portrait, unit_id);
        if (running != 0) {
            setRGB0(&page->portrait, 0x40, 0x50, 0x70);
        } else {
            setRGB0(&page->portrait, 0x80, 0x80, 0x80);
        }
        status->team_kind = 0;
        if (unit->initial_team_flags & BATTLE_TEAM_MASK) {
            status->team_kind = 1;
        }
        if (!(unit->initial_team_flags & BATTLE_TEAM_OR_PLAYER_CONTROL_MASK)) {
            status->team_kind = 2;
        }
        if (unit->auto_battle_setting != AI_AUTOBATTLE_MANUAL) {
            status->team_kind = 3;
        }
        params = g_battle_unit_status_sprite_image_params_by_mode[mode];
        if (g_battle_unit_status_bounce_step == 0) {
            setXY4(&page->portrait, frame_rect->x + 2, frame_rect->y + 2, frame_rect->x + 0x21, frame_rect->y + 2,
                frame_rect->x + 2, frame_rect->y + 0x32, frame_rect->x + 0x21, frame_rect->y + 0x32);
            mem_copy_bytes_nonempty(
                &params[6], &g_battle_unit_status_icon_image_params[status->team_kind], sizeof(*params));
            for (i = 0; i < 7; i++, params++) {
                gfx_init_image_loading(&page->sprites[i], &g_menu_texture_origin, frame_rect, params);
            }
        }
        if (mode != UNIT_STATS_PANEL_ATTACK_CASTER) {
            for (i = 0; i < 4; i++) {
                page->quads[i].clut = g_battle_unit_status_quad_clut_pairs[i][running & 1];
            }
        }
        poly = page->bars;
        if (mode == UNIT_STATS_PANEL_ATTACK_TARGET) {
            page->quads[1].clut = 0x7C3C;
            page->quads[2].clut = 0x7D7C;
        }
        color = g_battle_unit_status_gauge_colors;
        gauge = status->gauges;
        for (i = 0; i < 3; i++) {
            SetPolyG4(poly);
            extent_h = 0xC2;
            extent_w = frame_rect->x + 0x2F;
            if (mode == UNIT_STATS_PANEL_ATTACK_TARGET || mode == UNIT_STATS_PANEL_RIGHT_UNIT) {
                extent_w -= 0x6E;
                extent_h -= 0x10;
            }
            width = (gauge->value << 5) / gauge->max;
            if (width == 0) {
                extent_w = 0;
            }
            hide_values = g_menu_hide_numeric_values;
            if (hide_values != 0) {
                width = 0x20;
            }
            row_y = extent_h + i * 0xB;
            setXY4(poly, extent_w, row_y, extent_w + width, row_y, extent_w, row_y + 3, extent_w + width, row_y + 3);
            width = (gauge->bonus << 5) / gauge->max;
            if (width == 0) {
                extent_w = 0;
            }
            poly->x2 -= 3;
            poly->x3 -= 3;
            if (hide_values != 0) {
                setRGB0(poly, 0x40, 0x40, 0x40);
                setRGB1(poly, 0x80, 0x80, 0x80);
                setRGB2(poly, 0x40, 0x40, 0x40);
                setRGB3(poly, 0xA0, 0xC4, 0xC4);
            } else if (running != 0) {
                setRGB0(poly, color->r >> 1, color->g >> 1, color->b);
                color++;
                setRGB1(poly, color->r >> 1, color->g >> 1, color->b);
                color++;
                setRGB2(poly, color->r >> 1, color->g >> 1, color->b);
                color++;
                setRGB3(poly, color->r >> 1, color->g >> 1, color->b);
                color++;
            } else {
                setRGB0(poly, color->r, color->g, color->b);
                color++;
                setRGB1(poly, color->r, color->g, color->b);
                color++;
                setRGB2(poly, color->r, color->g, color->b);
                color++;
                setRGB3(poly, color->r, color->g, color->b);
                color++;
            }
            poly++;
            gauge++;
        }
        if (frame == 0) {
            buffer = menu_alloc_buffer(0x700);
            battle_clear_menu_render_buffer(buffer, 0x700);
            stride = &g_menu_text_state.pen.stride;
            origin = &g_menu_text_state.pen;
            *stride = 0x28;
            battle_menu_draw_numeric_display_entries(buffer, entries, origin, 6);
            LoadImage(&rects[0], (u32*)buffer);
            i = g_menu_hide_numeric_values;
            *stride = 0x60;
            g_menu_hide_numeric_values = 0;
            if (mode == UNIT_STATS_PANEL_HOVERED_UNIT || mode == UNIT_STATS_PANEL_RIGHT_UNIT) {
                battle_menu_draw_numeric_display_entries(buffer + 0x400, entries + 6, origin, 2);
                g_menu_hide_numeric_values = i;
                battle_menu_draw_numeric_display_entries(buffer + 0x400, entries + 8, origin, 2);
            } else if (mode == UNIT_STATS_PANEL_ATTACK_CASTER) {
                battle_menu_draw_numeric_display_entries(buffer + 0x400, entries + 6, origin, 2);
                g_menu_hide_numeric_values = i;
            } else if (text_id != 0) {
                battle_menu_set_text_origin(2, 0);
                battle_menu_display_text_string(buffer + 0x400, origin, text);
            }
            LoadImage(&rects[1], (u32*)(buffer + 0x400));
            text_loaded = 1;
        }
        offs = &offsets[frame & 1];
        base_y = (g_main_gfx_screen_polarity & 1) == 0 ? 0xF0 : 0;
        if (mode == UNIT_STATS_PANEL_ATTACK_TARGET || mode == UNIT_STATS_PANEL_RIGHT_UNIT) {
            offs->a.position.x = slide - 0x80;
        } else {
            offs->a.position.x = -0x80 - slide;
        }
        if (g_battle_unit_status_bounce_step != 0) {
            offs->a.position.y = base_y + g_battle_unit_status_bounce_offsets[g_battle_unit_status_bounce_step];
        } else {
            offs->a.position.y = base_y;
        }
        SetDrawOffset(&offs->a.packet, (u16*)&offs->a.position);
        offs->b.position.x = -0x80;
        offs->b.position.y = base_y;
        SetDrawOffset(&offs->b.packet, (u16*)&offs->b.position);
        gfx_draw_or_append_gpu_primitive(&offs->b);
        gfx_draw_or_append_gpu_primitive(&page->quads[0]);
        gfx_draw_or_append_gpu_primitive(&page->quads[1]);
        gfx_draw_or_append_gpu_primitive(&page->draw_mode_1);
        poly--;
        for (i = 0; i < 3; i++) {
            gfx_draw_or_append_gpu_primitive(poly);
            poly--;
        }
        gfx_draw_or_append_gpu_primitive(&page->quads[2]);
        if (mode == UNIT_STATS_PANEL_ATTACK_CASTER) {
            page->quads[4].clut = 0x7D7C;
            gfx_draw_or_append_gpu_primitive(&page->quads[4]);
        }
        if (mode != UNIT_STATS_PANEL_ATTACK_TARGET) {
            gfx_draw_or_append_gpu_primitive(&page->quads[3]);
        } else if (text_id != 0) {
            battle_menu_build_sprite_page(frame, frame_rect);
        }
        for (i = 0; i < 7; i++) {
            page->sprites[i].clut = g_battle_unit_status_sprite_clut_pairs[i][running & 1];
            gfx_draw_or_append_gpu_primitive(&page->sprites[i]);
        }
        gfx_draw_or_append_gpu_primitive(&page->draw_mode_0);
        gfx_draw_or_append_gpu_primitive(&page->portrait);
        if (running != 0) {
            menu_init_primitive_colors_dimmed(&page->palette);
        } else {
            menu_init_primitive_colors_normal(&page->palette);
        }
        menu_submit_numeric_display_frame_primitives(&page->palette);
        gfx_draw_or_append_gpu_primitive(&offs->a);
        thread_yield();
        g_menu_hide_numeric_values = unit->unit_flags & UNIT_FLAG_EGG;
        if (thread_get_current_task_id() == THREAD_TASK_HOVERED_UNIT_STATS) {
            g_battle_menu_hovered_stats_frame_count++;
        }
    }
exit:
    thread_yield();
    if (thread_get_current_task_id() == THREAD_TASK_HOVERED_UNIT_STATS) {
        g_battle_menu_hovered_stats_frame_count = 0;
    }
    battle_gfx_free_tpage7_vram(&rects[0]);
    battle_gfx_free_tpage7_vram(&rects[1]);
    thread_exit_current();
}
