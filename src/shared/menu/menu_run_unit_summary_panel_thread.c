#include "fft/battle_action.h"
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

/* Draw the unit summary and slide it out when the third thread parameter is set. */
void menu_run_unit_summary_panel_thread(void) {
    RECT rects[4];
    menu_unit_summary_frame_t frames[2];
    menu_unit_summary_frame_t* frame_record;
    unit_stats_t* unit;
    u8* buffer;
    u8* digits;
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
    digits = g_battle_unit_summary_digit_images;
    battle_menu_load_selected_unit();
    unit = battle_unit_get_attacker_data_pointer();
    g_menu_hide_numeric_values = unit->unit_flags & UNIT_FLAG_EGG;
    sprite = frames[0].sprites;
    battle_gfx_set_draw_mode_for_texture_page(&frames[0].draw_mode_0, MENU_TEXTURE_PAGE_FRAME);
    battle_gfx_set_draw_mode_for_texture_page(&frames[0].draw_mode_1, MENU_TEXTURE_PAGE_ALLOCATED_IMAGES);
    menu_init_sprite_array(sprite, 7, 0x7C3C);
    menu_init_default_poly_ft4(&frames[0].portrait);
    text_configure_sprite_vram(&rects[0], 0x58, 0x20, &frames[0].sprites[0], 0);
    text_configure_sprite_vram(&rects[1], 0x10, 0xA, &frames[0].sprites[1], 0);
    text_configure_sprite_vram(&rects[2], 0x10, 0xA, &frames[0].sprites[2], 0);
    text_configure_sprite_vram(&rects[3], 0x10, 0xA, &frames[0].sprites[3], 0);
    for (frame = 0; frame < 7; frame++) {
        gfx_init_image_loading(&frames[0].sprites[frame], &g_menu_texture_origin, &g_menu_unit_summary_panel_rect,
            &g_battle_unit_summary_sprite_image_params[frame]);
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
    battle_clear_menu_render_buffer(buffer, 0x580);
    stride = &g_menu_text_state.pen.stride;
    origin = &g_menu_text_state.pen;
    *stride = 0x58;
    battle_menu_set_text_origin(0, 0);
    battle_menu_display_text_string(buffer, origin, g_battle_menu_billboard_unit_name);
    battle_menu_set_text_origin(0, 0x10);
    battle_menu_display_text_string(buffer, origin, g_battle_menu_billboard_job_name);
    LoadImage(&rects[0], (u32*)buffer);
    battle_clear_menu_render_buffer(digits, 0xF0);
    *stride = 0x10;
    battle_menu_set_text_origin(0, 0);
    battle_text_draw_large_number_glyphs(g_menu_selected_unit.brave, 0x202, digits, origin);
    LoadImage(&rects[1], (u32*)digits);
    battle_menu_set_text_origin(0, 0);
    battle_text_draw_large_number_glyphs(g_menu_selected_unit.faith, 0x202, digits + 0x50, origin);
    LoadImage(&rects[2], (u32*)(digits + 0x50));
    battle_menu_set_text_origin(0, 0);
    if (g_main_debug_display_enabled != 0) {
        battle_text_draw_large_number_glyphs(g_menu_selected_unit.battle_id, 0x4002, digits + 0xA0, origin);
    } else if (g_menu_selected_unit.battle_id >= 0x10) {
        value = unit->formation_index;
        if (value == 0xFF) {
            value = 0;
        }
        battle_text_draw_large_number_glyphs(value + 1, 0x4002, digits + 0xA0, origin);
    }
    LoadImage(&rects[3], (u32*)(digits + 0xA0));
    frames[0].sprites[6].clut = 0x7C3C;
    frames[0].sprites[6].w = 0x18;
    frames[0].sprites[6].h = 0x14;
    zodiac = g_menu_selected_unit.zodiac;
    frames[0].sprites[6].u0 = zodiac % 7 * 0x18;
    frames[0].sprites[6].v0 = zodiac / 7 * 0x14 + 0x2A;
    mem_copy_bytes_nonempty(&frames[1], &frames[0], sizeof(menu_unit_summary_frame_t));
    frames[0].palette = &g_battle_unit_summary_palettes[0].primitives;
    frames[1].palette = &g_battle_unit_summary_palettes[1].primitives;
    battle_menu_init_numeric_display_frame_primitives(&g_menu_unit_summary_panel_rect, frames[0].palette);
    battle_menu_init_numeric_display_frame_primitives(&g_menu_unit_summary_panel_rect, frames[1].palette);

    for (frame = 0;; frame++) {
        thread_yield();
        g_menu_hide_numeric_values = battle_unit_get_attacker_data_pointer()->unit_flags & UNIT_FLAG_EGG;
        frame_record = &frames[frame & 1];
        if (frame == 0) {
            menu_free_memory(buffer);
        }
        value = (g_main_gfx_screen_polarity & 1) == 0 ? 0xF0 : 0;
        for (i = 0; i < g_script_event_speed; i++) {
            if (battle_thread_get_current_parameter_3() != 0) {
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
            if (g_battle_unit_status_bounce_step == 0) {
                g_battle_unit_status_bounce_step = 1;
            } else if (g_battle_unit_status_bounce_step < 6) {
                g_battle_unit_status_bounce_step = g_battle_unit_status_bounce_step + 1;
            }
        }
        if (g_menu_unit_summary_panel_rect.y == 0xAA) {
            if (g_battle_unit_status_bounce_step == 6) {
                g_battle_unit_status_bounce_step = 7;
            } else if (g_battle_unit_status_bounce_step >= 7 && g_battle_unit_status_bounce_step < 0xB) {
                g_battle_unit_status_bounce_step = g_battle_unit_status_bounce_step + 1;
            } else if (g_battle_unit_status_bounce_step == 0xB) {
                g_battle_unit_status_bounce_step = 0;
            }
        }
        if (g_battle_unit_status_bounce_step != 0) {
            frame_record->offset_a.y = value + g_battle_unit_status_bounce_offsets[g_battle_unit_status_bounce_step];
        } else {
            frame_record->offset_a.y = value;
        }
        frame_record->offset_a.x = slide - 0x80;
        SetDrawOffset(&frame_record->draw_offset_a, (u16*)&frame_record->offset_a);
        frame_record->offset_b.x = -0x80;
        frame_record->offset_b.y = value;
        SetDrawOffset(&frame_record->draw_offset_b, (u16*)&frame_record->offset_b);
        gfx_draw_or_append_gpu_primitive(&frame_record->draw_offset_b);
        value = battle_thread_find_running_by_task(THREAD_TASK_UNIT_EDITOR_PANEL);
        if (value != 0 && battle_thread_is_running_asm(value - 1) == 0) {
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
    battle_gfx_free_tpage7_vram(&rects[0]);
    battle_gfx_free_tpage7_vram(&rects[1]);
    battle_gfx_free_tpage7_vram(&rects[2]);
    battle_gfx_free_tpage7_vram(&rects[3]);
    thread_exit_current();
}
