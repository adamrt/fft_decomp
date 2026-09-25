#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/battle_text.h"
#include "fft/event.h"
#include "fft/main_gfx.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "fft/menu.h"
#include "fft/menu_types.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

extern void battle_world_display_specific_menu_text(s32 buffer, s32 origin, s32 text);

/* Unit summary panel thread, BATTLE twin of world_menu_unit_summary_panel_thread: renders the
 * unit's name/job, brave, faith and a third number into text images,
 * builds two alternating frames (seven sprites, the portrait quad and two draw
 * offsets), then slides the panel in and redraws it each frame until the
 * thread's third parameter asks it to slide out. A running task-0x15 thread
 * dims the panel.
 *
 * `frame` doubles as the sprite-setup counter and `value` holds the number
 * shown, then the vertical draw offset, then the task-0x15 thread id; the
 * target keeps each of those roles in one callee-saved register. */
void battle_menu_run_unit_summary_panel_thread(void) {
    RECT rects[4];
    world_unit_summary_frame_t frames[2];
    world_unit_summary_frame_t* frame_record;
    battle_stats_t* unit;
    u8* buffer;
    u8* digits;
    s32* stride;
    battle_rect_t* origin;
    SPRT* sprite;
    s32 speed;
    s32 slide;
    s32 frame;
    s32 i;
    s32 value;
    s32 zodiac;

    speed = 0x16;
    slide = 0x84;
    digits = g_battle_unit_summary_digit_images;
    battle_menu_store_units_small_in_battle_display_data();
    unit = battle_unit_get_attacker_data_pointer();
    g_battle_menu_hide_numeric_values = unit->unit_flags & 4;
    sprite = frames[0].sprites;
    battle_gfx_set_draw_mode_for_texture_page(&frames[0].draw_mode_0, 0);
    battle_gfx_set_draw_mode_for_texture_page(&frames[0].draw_mode_1, 1);
    battle_menu_init_sprite_array(sprite, 7, 0x7C3C);
    battle_gfx_init_default_poly_ft4(&frames[0].portrait);
    battle_text_configure_sprite_vram(&rects[0], 0x58, 0x20, &frames[0].sprites[0], 0);
    battle_text_configure_sprite_vram(&rects[1], 0x10, 0xA, &frames[0].sprites[1], 0);
    battle_text_configure_sprite_vram(&rects[2], 0x10, 0xA, &frames[0].sprites[2], 0);
    battle_text_configure_sprite_vram(&rects[3], 0x10, 0xA, &frames[0].sprites[3], 0);
    for (frame = 0; frame < 7; frame++) {
        battle_gfx_init_image_loading((POLY_FT4*)&frames[0].sprites[frame], &g_battle_menu_texture_location,
            (const battle_image_location_t*)&g_battle_unit_summary_panel_rect,
            &g_battle_unit_summary_sprite_image_params[frame]);
    }
    battle_gfx_init_image_loading(&frames[0].portrait, &g_battle_menu_texture_location,
        (const battle_image_location_t*)&g_battle_unit_summary_panel_rect,
        &g_battle_unit_summary_portrait_image_params);
    if (unit->initial_team_flags & 0x30) {
        frames[0].portrait.clut = 0x7FFD;
    } else {
        frames[0].portrait.clut = 0x7FBD;
    }
    frames[0].portrait.tpage = GetTPage(0, 1, 0x3C0, 0x100);
    SetSemiTrans(&frames[0].portrait, 1);
    buffer = battle_menu_alloc_buffer(0x580);
    battle_clear_menu_render_buffer(buffer, 0x580);
    stride = &g_menu_inner_window_width;
    *stride = 0x58;
    battle_menu_set_text_origin(0, 0);
    origin = (battle_rect_t*)(stride - 2);
    battle_world_display_specific_menu_text((s32)buffer, (s32)origin, (s32)g_battle_menu_billboard_unit_name);
    battle_menu_set_text_origin(0, 0x10);
    battle_world_display_specific_menu_text((s32)buffer, (s32)origin, (s32)g_battle_menu_billboard_job_name);
    LoadImage(&rects[0], (u32*)buffer);
    battle_clear_menu_render_buffer(digits, 0xF0);
    *stride = 0x10;
    battle_menu_set_text_origin(0, 0);
    battle_text_draw_large_number_glyphs(g_battle_active_turn_unit.brave, 0x202, digits, origin);
    LoadImage(&rects[1], (u32*)digits);
    battle_menu_set_text_origin(0, 0);
    battle_text_draw_large_number_glyphs(g_battle_active_turn_unit.faith, 0x202, digits + 0x50, origin);
    LoadImage(&rects[2], (u32*)(digits + 0x50));
    battle_menu_set_text_origin(0, 0);
    if (g_main_debug_display_enabled != 0) {
        battle_text_draw_large_number_glyphs(g_battle_active_turn_unit.battle_id, 0x4002, digits + 0xA0, origin);
    } else if (g_battle_active_turn_unit.battle_id >= 0x10) {
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
    zodiac = g_battle_active_turn_unit.zodiac;
    frames[0].sprites[6].u0 = (s16)(zodiac % 7) * 0x18;
    frames[0].sprites[6].v0 = (s16)(zodiac / 7) * 0x14 + 0x2A;
    battle_copy_bytes(&frames[1], &frames[0], sizeof(world_unit_summary_frame_t));
    frames[0].palette = &g_battle_unit_summary_palettes[0].primitives;
    frames[1].palette = &g_battle_unit_summary_palettes[1].primitives;
    battle_menu_init_numeric_display_frame_primitives(&g_battle_unit_summary_panel_rect, frames[0].palette);
    battle_menu_init_numeric_display_frame_primitives(&g_battle_unit_summary_panel_rect, frames[1].palette);

    for (frame = 0;; frame++) {
        battle_thread_yield();
        g_battle_menu_hide_numeric_values = battle_unit_get_attacker_data_pointer()->unit_flags & 4;
        frame_record = &frames[frame & 1];
        if (frame == 0) {
            battle_menu_free_memory(buffer);
        }
        value = (g_main_gfx_screen_polarity & 1) == 0 ? 0xF0 : 0;
        for (i = 0; i < g_battle_event_speed; i++) {
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
        if (g_battle_unit_summary_panel_rect.y == 0x1A) {
            if (g_battle_unit_status_bounce_step == 0) {
                g_battle_unit_status_bounce_step = 1;
            } else if (g_battle_unit_status_bounce_step < 6) {
                g_battle_unit_status_bounce_step = g_battle_unit_status_bounce_step + 1;
            }
        }
        if (g_battle_unit_summary_panel_rect.y == 0xAA) {
            if (g_battle_unit_status_bounce_step == 6) {
                g_battle_unit_status_bounce_step = 7;
            } else if (g_battle_unit_status_bounce_step >= 7 && g_battle_unit_status_bounce_step < 0xB) {
                g_battle_unit_status_bounce_step = g_battle_unit_status_bounce_step + 1;
            } else if (g_battle_unit_status_bounce_step == 0xB) {
                g_battle_unit_status_bounce_step = 0;
            }
        }
        if (g_battle_unit_status_bounce_step != 0) {
            frame_record->offset_a[1] = value + g_battle_unit_status_bounce_offsets[g_battle_unit_status_bounce_step];
        } else {
            frame_record->offset_a[1] = value;
        }
        frame_record->offset_a[0] = slide - 0x80;
        SetDrawOffset(frame_record->draw_offset_a, frame_record->offset_a);
        frame_record->offset_b[0] = -0x80;
        frame_record->offset_b[1] = value;
        SetDrawOffset(frame_record->draw_offset_b, frame_record->offset_b);
        battle_gfx_draw_or_append_gpu_primitive(frame_record->draw_offset_b);
        value = battle_thread_find_running_by_task(NATIVE_THREAD_TASK_UNIT_EDITOR_PANEL);
        if (value != 0 && battle_thread_is_running_8014cc94(value - 1) == 0) {
            value = 0;
        }
        if (value != 0) {
            frame_record->portrait.r0 = 0x40;
            frame_record->portrait.g0 = 0x40;
            frame_record->portrait.b0 = 0x60;
        } else {
            frame_record->portrait.r0 = 0x80;
            frame_record->portrait.g0 = 0x80;
            frame_record->portrait.b0 = 0x80;
        }
        SetSemiTrans(&frame_record->portrait, 1);
        battle_gfx_draw_or_append_gpu_primitive(&frame_record->portrait);
        for (i = 3; i >= 0; i--) {
            if (value != 0) {
                frame_record->sprites[i].clut = 0x7D3C;
            } else {
                frame_record->sprites[i].clut = 0x7C3C;
            }
            battle_gfx_draw_or_append_gpu_primitive(&frame_record->sprites[i]);
        }
        battle_gfx_draw_or_append_gpu_primitive(&frame_record->draw_mode_1);
        for (i = 4; i < 7; i++) {
            if (value != 0) {
                frame_record->sprites[i].clut = 0x7D3C;
            } else {
                frame_record->sprites[i].clut = 0x7C3C;
            }
            battle_gfx_draw_or_append_gpu_primitive(&frame_record->sprites[i]);
        }
        battle_gfx_draw_or_append_gpu_primitive(&frame_record->draw_mode_0);
        if (value != 0) {
            battle_menu_init_primitive_colors_palette_bank_1(frame_record->palette);
        } else {
            battle_menu_init_primitive_colors_palette_bank_0(frame_record->palette);
        }
        battle_menu_submit_numeric_display_frame_primitives(frame_record->palette);
        battle_gfx_draw_or_append_gpu_primitive(frame_record->draw_offset_a);
    }
exit:
    battle_thread_yield();
    battle_gfx_free_tpage7_vram(&rects[0]);
    battle_gfx_free_tpage7_vram(&rects[1]);
    battle_gfx_free_tpage7_vram(&rects[2]);
    battle_gfx_free_tpage7_vram(&rects[3]);
    battle_thread_exit_current();
}
