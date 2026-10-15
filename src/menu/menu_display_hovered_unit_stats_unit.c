#include "fft/battle_action.h"
#include "fft/battle_gfx.h"
#include "fft/battle_menu.h"
#include "fft/battle_text.h"
#include "fft/battle_unit.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

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
        g_menu_hovered_stats_frame_count++;
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
            g_menu_sprite_page_image_params[25].x_screen_offset
                = g_menu_sprite_page_image_params[28].x_screen_offset + extent_w;
            g_menu_sprite_page_image_params[26].x_screen_offset
                = g_menu_sprite_page_image_params[29].x_screen_offset + extent_w;
            for (frame = 0; frame < 3; frame++) {
                g_menu_sprite_page_image_params[frame].x_screen_offset
                    = g_menu_sprite_page_image_param_bases[frame].x_screen_offset + extent_w;
                g_menu_sprite_page_image_params[frame].width
                    = g_menu_sprite_page_image_param_bases[frame].width - extent_w;
            }
            g_menu_sprite_page_image_params[3].x_screen_offset
                = g_menu_sprite_page_image_params[8].x_screen_offset + extent_w;
        }
    }
    page = &g_menu_unit_status_panel_pages[g_menu_unit_status_first_page_by_mode[mode]];
    menu_init_numeric_display_frame_primitives(frame_rect, &page->palette);
    gfx_set_draw_mode_for_texture_page(&page->draw_mode_1, MENU_TEXTURE_PAGE_ALLOCATED_IMAGES);
    gfx_set_draw_mode_for_texture_page(&page->draw_mode_0, MENU_TEXTURE_PAGE_FRAME);
    menu_init_sprite_array(page->sprites, 7, 0x7CBC);
    for (frame = 0; frame < 5; frame++) {
        menu_init_default_poly_ft4(&page->quads[frame]);
    }
    menu_init_default_poly_ft4(&page->portrait);
    text_configure_sprite_vram(&rects[0], 0x28, 0x28, &page->quads[0], 2);
    text_configure_sprite_vram(&rects[1], 0x60, 0x10, &page->quads[1], 2);
    params = g_menu_unit_status_quad_image_params_by_mode[mode];
    if (mode == UNIT_STATS_PANEL_ATTACK_CASTER) {
        gfx_init_image_loading(&page->quads[4], &g_menu_texture_origin, frame_rect, &params[4]);
        for (frame = 0; frame < 2; frame++, params++) {
            gfx_init_image_loading(&page->quads[frame], &g_menu_texture_origin, frame_rect, params);
            page->quads[frame].clut = 0x7CBC;
        }
        menu_display_projected_action_effect_variant(&page->quads[frame], &g_menu_texture_origin, frame_rect, params);
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
            if (thread_get_current_parameter_3_variant() != 0) {
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
        running = thread_find_running_by_task(THREAD_TASK_UNIT_EDITOR_PANEL);
        if (running != 0 && thread_is_running_asm_variant(running - 1) == 0) {
            running = 0;
        }
        page = &g_menu_unit_status_panel_pages[g_menu_unit_status_first_page_by_mode[mode] + (frame & 1)];
        menu_build_unit_portrait_poly_variant(&page->portrait, unit_id);
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
        params = g_menu_unit_status_sprite_image_params_by_mode[mode];
        if (g_menu_unit_status_bounce_step == 0) {
            setXY4(&page->portrait, frame_rect->x + 2, frame_rect->y + 2, frame_rect->x + 0x21, frame_rect->y + 2,
                frame_rect->x + 2, frame_rect->y + 0x32, frame_rect->x + 0x21, frame_rect->y + 0x32);
            mem_copy_bytes_nonempty(
                &params[6], &g_menu_unit_status_icon_image_params[status->team_kind], sizeof(*params));
            for (i = 0; i < 7; i++, params++) {
                gfx_init_image_loading(&page->sprites[i], &g_menu_texture_origin, frame_rect, params);
            }
        }
        if (mode != UNIT_STATS_PANEL_ATTACK_CASTER) {
            for (i = 0; i < 4; i++) {
                page->quads[i].clut = g_menu_unit_status_quad_clut_pairs[i][running & 1];
            }
        }
        poly = page->bars;
        if (mode == UNIT_STATS_PANEL_ATTACK_TARGET) {
            page->quads[1].clut = 0x7C3C;
            page->quads[2].clut = 0x7D7C;
        }
        color = g_menu_unit_status_gauge_colors;
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
            menu_clear_render_buffer(buffer, 0x700);
            stride = &g_menu_text_state.pen.stride;
            origin = &g_menu_text_state.pen;
            *stride = 0x28;
            menu_draw_numeric_display_entries(buffer, entries, origin, 6);
            LoadImage(&rects[0], (u32*)buffer);
            i = g_menu_hide_numeric_values;
            *stride = 0x60;
            g_menu_hide_numeric_values = 0;
            if (mode == UNIT_STATS_PANEL_HOVERED_UNIT || mode == UNIT_STATS_PANEL_RIGHT_UNIT) {
                menu_draw_numeric_display_entries(buffer + 0x400, entries + 6, origin, 2);
                g_menu_hide_numeric_values = i;
                menu_draw_numeric_display_entries(buffer + 0x400, entries + 8, origin, 2);
            } else if (mode == UNIT_STATS_PANEL_ATTACK_CASTER) {
                menu_draw_numeric_display_entries(buffer + 0x400, entries + 6, origin, 2);
                g_menu_hide_numeric_values = i;
            } else if (text_id != 0) {
                menu_set_text_origin(2, 0);
                menu_display_text_string(buffer + 0x400, origin, text);
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
        if (g_menu_unit_status_bounce_step != 0) {
            offs->a.position.y = base_y + g_menu_unit_status_bounce_offsets[g_menu_unit_status_bounce_step];
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
            menu_build_sprite_page(frame, frame_rect);
        }
        for (i = 0; i < 7; i++) {
            page->sprites[i].clut = g_menu_unit_status_sprite_clut_pairs[i][running & 1];
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
            g_menu_hovered_stats_frame_count++;
        }
    }
exit:
    thread_yield();
    if (thread_get_current_task_id() == THREAD_TASK_HOVERED_UNIT_STATS) {
        g_menu_hovered_stats_frame_count = 0;
    }
    gfx_free_texture_rect(&rects[0]);
    gfx_free_texture_rect(&rects[1]);
    thread_exit_current();
}

/* Reload the unit and redraw its status panel; a negative list index draws dashes. */
void menu_refresh_hovered_unit_stats_display(void) {
    menu_load_selected_unit_variant();
    g_menu_hovered_unit_stats_setup[6].format = 2;
    if (g_menu_selected_unit_status.list_index < 0) {
        g_menu_hovered_unit_stats_setup[6].format = 0x0c00;
    }
    menu_display_hovered_unit_stats(&g_menu_hovered_unit_stats_rect, UNIT_STATS_PANEL_HOVERED_UNIT,
        g_menu_hovered_unit_stats_setup, &g_menu_selected_unit_status);
}

/* Reload the unit and redraw its status panel; a negative list index draws dashes. */
void menu_refresh_right_unit_stats_display(void) {
    menu_load_selected_unit_variant();
    g_menu_right_unit_stats_setup[6].format = 2;
    if (g_menu_comparison_unit_status.list_index < 0) {
        g_menu_right_unit_stats_setup[6].format = 0x0c00;
    }
    menu_display_hovered_unit_stats(&g_menu_right_unit_stats_rect, UNIT_STATS_PANEL_RIGHT_UNIT,
        g_menu_right_unit_stats_setup, &g_menu_comparison_unit_status);
}

/* Preview the pending action, then display its caster panel. */
void menu_preview_attack_caster_stats(void) {
    unit_stats_t* attacker;
    unit_stats_t* target;
    action_command_t* action;
    ability_data_t* ability_data;
    main_ability_secondary_pointer_t secondary;
    s16* selected;

    attacker = menu_get_selected_unit_stats();
    target = battle_unit_get_stats_from_battle_id(g_menu_comparison_unit_data.battle_id);
    /* One address register serves both reads, as in the target. */
    selected = &g_menu_selected_unit.battle_id;
    action = battle_unit_get_runtime_action_command_by_battle_id(*selected);
    action->actor_id = *selected;
    if (action->ability_id < ABILITY_ID_ITEM_FIRST) {
        main_ability_calculate_pointers_and_type(action->ability_id & ABILITY_ID_MASK, &ability_data, &secondary);
    }
    /* Item IDs leave secondary unset before this range-flags read. */
    if (((secondary.range->flags_3 & ABILITY_SECONDARY_FLAG_3_PERSEVERE) && action->ability_id < ABILITY_ID_ITEM_FIRST)
        || action->skillset_id == SKILLSET_ID_EQUIP_CHANGE) {
        g_menu_preview_target_action = 0;
    } else {
        g_thread_call_target = (void (*)(void))action_calculate_projected_effect;
        thread_call_on_main_stack(attacker, target, action);
        g_menu_preview_target_action = &target->action;
    }
    thread_yield();
    thread_yield();
    menu_load_selected_unit_variant();
    menu_display_hovered_unit_stats(&g_menu_attack_caster_stats_rect, UNIT_STATS_PANEL_ATTACK_CASTER,
        g_menu_attack_caster_stats_setup, &g_menu_selected_unit_status);
}

/* Load the preview target, restore the active unit, then display its panel. */
void menu_preview_attack_target_stats(void) {
    s32 saved;

    if (g_thread_contexts[THREAD_SLOT_RIGHT_UNIT_STATS].task_id != THREAD_TASK_ATTACK_CASTER_STATS) {
        thread_wait_until_inactive(THREAD_SLOT_RIGHT_UNIT_STATS);
    }
    thread_yield();
    thread_yield();
    saved = g_menu_selected_unit.battle_id;
    /* The target loads the preview unit id unsigned (lhu). */
    g_menu_selected_unit.battle_id = g_menu_comparison_unit_data.battle_id;
    menu_load_selected_unit_variant();
    g_menu_selected_unit.battle_id = saved;
    g_menu_comparison_unit_fields.state.generic_monster = g_menu_selected_unit_fields.state.generic_monster;
    g_menu_comparison_unit_fields.state.ability_ids[2] = g_menu_selected_unit_fields.state.ability_ids[2];
    menu_load_selected_unit_variant();
    menu_display_hovered_unit_stats(&g_menu_attack_target_stats_rect, UNIT_STATS_PANEL_ATTACK_TARGET,
        g_menu_attack_target_stats_setup, &g_menu_comparison_unit_status);
}

/* Draw the unit summary and slide it out when the third thread parameter is set. */
void menu_run_unit_summary_panel_thread(void) {
    RECT rects[4];
    menu_unit_summary_frame_t frames[2];
    menu_unit_summary_frame_t* frame_record;
    unit_stats_t* unit;
    u8* buffer;
    menu_unit_number_images_t* digits;
    glyph_blit_t* origin;
    s32* stride;
    SPRT* sprite;
    s32 speed;
    s32 slide;
    s32 frame;
    s32 i;
    s32 value;
    s16 zodiac;

    speed = 0x16;
    slide = 0x84;
    digits = &g_menu_unit_summary_digit_images;
    menu_load_selected_unit_variant();
    unit = menu_get_selected_unit_stats();
    g_menu_hide_numeric_values = unit->unit_flags & UNIT_FLAG_EGG;
    sprite = frames[0].sprites;
    gfx_set_draw_mode_for_texture_page(&frames[0].draw_mode_0, MENU_TEXTURE_PAGE_FRAME);
    gfx_set_draw_mode_for_texture_page(&frames[0].draw_mode_1, MENU_TEXTURE_PAGE_ALLOCATED_IMAGES);
    menu_init_sprite_array(sprite, 7, 0x7C3C);
    menu_init_default_poly_ft4(&frames[0].portrait);
    text_configure_sprite_vram(&rects[0], 0x58, 0x20, &frames[0].sprites[0], 0);
    text_configure_sprite_vram(&rects[1], 0x10, 0xA, &frames[0].sprites[1], 0);
    text_configure_sprite_vram(&rects[2], 0x10, 0xA, &frames[0].sprites[2], 0);
    text_configure_sprite_vram(&rects[3], 0x10, 0xA, &frames[0].sprites[3], 0);
    for (frame = 0; frame < 7; frame++) {
        gfx_init_image_loading(&frames[0].sprites[frame], &g_menu_texture_origin, &g_menu_unit_summary_panel_rect,
            &g_menu_unit_summary_sprite_image_params[frame]);
    }
    gfx_init_image_loading(&frames[0].portrait, &g_menu_texture_origin, &g_menu_unit_summary_panel_rect,
        &g_battle_unit_summary_portrait_image_params);
    if (unit->initial_team_flags & BATTLE_TEAM_MASK) {
        frames[0].portrait.clut = 0x7FFD;
    } else {
        frames[0].portrait.clut = 0x7FBD;
    }
    frames[0].portrait.tpage = GetTPage(0, 1, 0x3C0, 0x100);
    SetSemiTrans(&frames[0].portrait, 1);
    buffer = menu_alloc_buffer(0x580);
    menu_clear_render_buffer(buffer, 0x580);
    stride = &g_menu_text_state.pen.stride;
    origin = &g_menu_text_state.pen;
    *stride = 0x58;
    menu_set_text_origin(0, 0);
    menu_display_text_string(buffer, origin, g_menu_billboard_unit_name);
    menu_set_text_origin(0, 0x10);
    menu_display_text_string(buffer, origin, g_menu_billboard_job_name);
    LoadImage(&rects[0], (u32*)buffer);
    menu_clear_render_buffer(digits, sizeof(*digits));
    *stride = 0x10;
    menu_set_text_origin(0, 0);
    text_draw_large_number_glyphs_variant(g_menu_selected_unit.brave, 0x202, digits->brave, origin);
    LoadImage(&rects[1], (u32*)digits->brave);
    menu_set_text_origin(0, 0);
    text_draw_large_number_glyphs_variant(g_menu_selected_unit.faith, 0x202, digits->faith, origin);
    LoadImage(&rects[2], (u32*)digits->faith);
    menu_set_text_origin(0, 0);
    if (g_main_debug_display_enabled != 0) {
        text_draw_large_number_glyphs_variant(g_menu_selected_unit.battle_id, 0x4002, digits->roster_slot, origin);
    } else if (g_menu_selected_unit.battle_id >= 0x10) {
        value = unit->formation_index;
        if (value == 0xFF) {
            value = 0;
        }
        text_draw_large_number_glyphs_variant(value + 1, 0x4002, digits->roster_slot, origin);
    }
    LoadImage(&rects[3], (u32*)digits->roster_slot);
    frames[0].sprites[6].clut = 0x7C3C;
    frames[0].sprites[6].w = 0x18;
    frames[0].sprites[6].h = 0x14;
    zodiac = g_menu_selected_unit.zodiac;
    frames[0].sprites[6].u0 = zodiac % 7 * 0x18;
    frames[0].sprites[6].v0 = zodiac / 7 * 0x14 + 0x2A;
    mem_copy_bytes_nonempty(&frames[1], &frames[0], sizeof(menu_unit_summary_frame_t));
    frames[0].palette = &g_menu_unit_summary_palettes[0].primitives;
    frames[1].palette = &g_menu_unit_summary_palettes[1].primitives;
    menu_init_numeric_display_frame_primitives(&g_menu_unit_summary_panel_rect, frames[0].palette);
    menu_init_numeric_display_frame_primitives(&g_menu_unit_summary_panel_rect, frames[1].palette);

    for (frame = 0;; frame++) {
        thread_yield();
        g_menu_hide_numeric_values = menu_get_selected_unit_stats()->unit_flags & UNIT_FLAG_EGG;
        frame_record = &frames[frame & 1];
        if (frame == 0) {
            menu_free_memory(buffer);
        }
        value = (g_main_gfx_screen_polarity & 1) == 0 ? 0xF0 : 0;
        for (i = 0; i < g_script_event_speed; i++) {
            if (thread_get_current_parameter_3_variant() != 0) {
                slide += 0x1E;
                if (slide > 0x80) {
                    /* Leaves both the step loop and the frame loop. */
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
        if (g_menu_unit_summary_panel_rect.y == 0x1A) {
            if (g_menu_unit_status_bounce_step == 0) {
                g_menu_unit_status_bounce_step = 1;
            } else if (g_menu_unit_status_bounce_step < 6) {
                g_menu_unit_status_bounce_step = g_menu_unit_status_bounce_step + 1;
            }
        }
        if (g_menu_unit_summary_panel_rect.y == 0xAA) {
            if (g_menu_unit_status_bounce_step == 6) {
                g_menu_unit_status_bounce_step = 7;
            } else if (g_menu_unit_status_bounce_step >= 7 && g_menu_unit_status_bounce_step < 0xB) {
                g_menu_unit_status_bounce_step = g_menu_unit_status_bounce_step + 1;
            } else if (g_menu_unit_status_bounce_step == 0xB) {
                g_menu_unit_status_bounce_step = 0;
            }
        }
        if (g_menu_unit_status_bounce_step != 0) {
            frame_record->offset_a.y = value + g_menu_unit_status_bounce_offsets[g_menu_unit_status_bounce_step];
        } else {
            frame_record->offset_a.y = value;
        }
        frame_record->offset_a.x = slide - 0x80;
        SetDrawOffset(&frame_record->draw_offset_a, (u16*)&frame_record->offset_a);
        frame_record->offset_b.x = -0x80;
        frame_record->offset_b.y = value;
        SetDrawOffset(&frame_record->draw_offset_b, (u16*)&frame_record->offset_b);
        gfx_draw_or_append_gpu_primitive(&frame_record->draw_offset_b);
        value = thread_find_running_by_task(THREAD_TASK_UNIT_EDITOR_PANEL);
        if (value != 0 && thread_is_running_asm_variant(value - 1) == 0) {
            value = 0;
        }
        if (value != 0) {
            setRGB0(&frame_record->portrait, 0x40, 0x40, 0x60);
        } else {
            setRGB0(&frame_record->portrait, 0x80, 0x80, 0x80);
        }
        SetSemiTrans(&frame_record->portrait, 1);
        gfx_draw_or_append_gpu_primitive(&frame_record->portrait);
        for (i = 3; i >= 0; i--) {
            if (value != 0) {
                frame_record->sprites[i].clut = 0x7D3C;
            } else {
                frame_record->sprites[i].clut = 0x7C3C;
            }
            gfx_draw_or_append_gpu_primitive(&frame_record->sprites[i]);
        }
        gfx_draw_or_append_gpu_primitive(&frame_record->draw_mode_1);
        for (i = 4; i < 7; i++) {
            if (value != 0) {
                frame_record->sprites[i].clut = 0x7D3C;
            } else {
                frame_record->sprites[i].clut = 0x7C3C;
            }
            gfx_draw_or_append_gpu_primitive(&frame_record->sprites[i]);
        }
        gfx_draw_or_append_gpu_primitive(&frame_record->draw_mode_0);
        if (value != 0) {
            menu_init_primitive_colors_dimmed(frame_record->palette);
        } else {
            menu_init_primitive_colors_normal(frame_record->palette);
        }
        menu_submit_numeric_display_frame_primitives(frame_record->palette);
        gfx_draw_or_append_gpu_primitive(&frame_record->draw_offset_a);
    }
exit:
    thread_yield();
    gfx_free_texture_rect(&rects[0]);
    gfx_free_texture_rect(&rects[1]);
    gfx_free_texture_rect(&rects[2]);
    gfx_free_texture_rect(&rects[3]);
    thread_exit_current();
}

/* Fill the texture coordinates, CLUT and size of a 16x16 item icon sprite. */
void gfx_set_item_icon_sprite(SPRT* sprite, s32 item_id) {
    item_data_t* item;
    s32 sprite_id;
    s32 palette;
    s32 column;

    item = main_item_get_data_pointer(item_id);
    sprite_id = item->sprite_id;
    palette = item->palette;
    column = sprite_id % 15;
    sprite->u0 = column << 4;
    sprite->v0 = ((sprite_id / 15) << 4) + 0x20;
    sprite->clut
        = GetClut(g_gfx_item_icon_clut_base_x + ((palette % 8) << 4), g_gfx_item_icon_clut_base_y + palette / 8);
    sprite->w = 16;
    sprite->h = 16;
}
