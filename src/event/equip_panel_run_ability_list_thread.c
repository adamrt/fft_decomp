#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/equip.h"
#include "fft/main_gfx.h"
#include "fft/menu.h"
#include "fft/menu_types.h"
#include "fft/thread.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Same double-buffered battle_menu_status_panel_buffer_t frame as equip_panel_run_equipment_list_thread and the
 * WORLD twin world_menu_ability_panel_thread. */

/* Thread record; the origin is read as words for the draw-area offset. */
typedef struct equip_ability_panel_thread {
    s32 x; /* 0x00 */
    s32 y; /* 0x04 */
    s32 unknown_08;
    s32 redraw_request; /* 0x0c */
} equip_ability_panel_thread_t;

void battle_gfx_init_image_loading(POLY_FT4* primitive, const battle_image_location_t* base_load,
    const battle_image_location_t* base_screen, const world_gfx_image_load_parameters_t* parameters);

/* Two-column ability panel thread (task 0x3b); EQUIP twin of WORLD
 * world_menu_ability_panel_thread. Thread 10 draws the unit editor stats into the first
 * frame pair, other threads draw g_equip_item_preview_stat_detail into the second. */
void equip_panel_run_ability_list_thread(void) {
    s32 offset[2];
    /* Never referenced: the target frame has these 16 bytes between the
       offset pair and the spill slots of u/base/upload. */
    s32 unused[4];
    s32 u;
    battle_menu_status_panel_buffer_t* base;
    RECT* upload;
    equip_ability_panel_thread_t* thread;
    u8* text;
    battle_menu_status_panel_buffer_t* screen;
    battle_menu_status_panel_slot_state_t* state;
    s32 frame;
    s32 i;
    s32 id;

    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_STATUS_PANEL);
    thread = (equip_ability_panel_thread_t*)g_battle_threads[g_battle_current_thread_id].function_parameter_1;
    g_equip_input_controller = battle_script_get_controller_input_pointer(0);
    if (g_battle_current_thread_id == 10) {
        text = g_equip_panel_text_image_a;
        screen = g_equip_panel_frames_a;
        state = (battle_menu_status_panel_slot_state_t*)g_equip_unit_editor_stats;
        base = g_equip_panel_frames_a;
        upload = &g_equip_panel_text_upload_rect_a;
        u = 0;
    } else {
        text = g_equip_panel_text_image_b;
        screen = g_equip_panel_frames_b;
        state = (battle_menu_status_panel_slot_state_t*)&g_equip_item_preview_stat_detail;
        upload = &g_equip_panel_text_upload_rect_b;
        base = g_equip_panel_frames_b;
        u = 0x50;
    }
    battle_menu_init_numeric_display_frame_primitives(&g_equip_right_panel_frame_rect, &screen->numeric_frame);
    equip_gfx_init_menu_tile_and_line_primitives(screen);
    equip_gfx_init_scaled_draw_area_packets(&screen->portrait);
    /* The tile/line fixups must read g_equip_panel_origin_offsets as an array
     * element: a cast of a u8[] view folds the loop's y reads to absolute
     * addresses instead of the target's pointer-relative loads. */
    screen->tiles[1].x0 += g_equip_panel_origin_offsets[0].x;
    screen->tiles[1].y0 += g_equip_panel_origin_offsets[0].y;
    for (i = 4; i < 8; i++) {
        screen->lines[i].x0 += g_equip_panel_origin_offsets[0].x;
        screen->lines[i].y0 += g_equip_panel_origin_offsets[0].y;
        screen->lines[i].x1 += g_equip_panel_origin_offsets[0].x;
        screen->lines[i].y1 += g_equip_panel_origin_offsets[0].y;
    }
    battle_menu_init_sprite_array(&screen->sprites[0], 12, 0x7d7c);
    battle_menu_init_sprite_array(&screen->sprites[12], 7, 0x7c3c);
    screen->sprites[0].clut = 0x7cbc;
    screen->sprites[1].clut = 0x7cbc;
    battle_copy_bytes(&screen[1], screen, sizeof(battle_menu_status_panel_buffer_t));

    frame = 0;
    for (;;) {
        screen = &base[frame & 1];
        if (state->generic_monster == 0) {
            for (i = 0; i < 19; i++) {
                battle_gfx_init_image_loading((POLY_FT4*)&screen->sprites[i], g_equip_editor_numeric_geometry,
                    (const battle_image_location_t*)g_equip_panel_origin_offsets,
                    g_equip_panel_label_layouts_mode0 + i);
            }
        } else {
            for (i = 0; i < 19; i++) {
                battle_gfx_init_image_loading((POLY_FT4*)&screen->sprites[i], g_equip_editor_numeric_geometry,
                    (const battle_image_location_t*)g_equip_panel_origin_offsets,
                    g_equip_panel_label_layouts_mode1 + i);
            }
        }
        if (state->generic_monster != 0) {
            for (i = 0; i < 5; i++) {
                u16 ability = state->ability_ids[i];
                if (ability == BATTLE_MENU_STATUS_PANEL_LABEL_NONE || ability == 0) {
                    screen->sprites[7 + i].x0 -= 0x200;
                }
            }
        }
        /* The explicit reset lengthens i's live range, which keeps it in $s2. */
        i = 0;
        if (frame == 0 || thread->redraw_request == 1) {
            battle_clear_menu_render_buffer(text, 0xc80);
            g_menu_inner_window_width = 0x50;
            battle_menu_set_text_origin(0, 0);
            for (i = 0; i < 2; i++) {
                id = state->ability_ids[i] + 0xb000;
                if (state->generic_monster != 0) {
                    id = state->ability_ids[i] + 0x7000;
                }
                if ((u16)state->ability_ids[i] == BATTLE_MENU_STATUS_PANEL_LABEL_NONE) {
                    g_menu_text_state.origin_y += 0x10;
                } else {
                    battle_menu_display_text_entry(id, text, &g_menu_text_state.origin_x);
                    g_menu_text_state.origin_y += 0x10;
                }
            }
            for (; i < 5; i++) {
                if ((u16)state->ability_ids[i] != BATTLE_MENU_STATUS_PANEL_LABEL_NONE) {
                    battle_menu_display_text_entry(state->ability_ids[i] + 0x7000, text, &g_menu_text_state.origin_x);
                }
                g_menu_text_state.origin_y += 0x10;
            }
            LoadImage(upload, (u32*)text);
            thread->redraw_request = 0;
        }
        screen->sprites[12].u0 = u;
        screen->sprites[13].u0 = u;
        screen->draw_offsets[0].x = thread->x - 0xf3;
        g_equip_gfx_draw_offset_y = g_main_gfx_screen_polarity * 0xf0 != 0xf0 ? 0xf0 : 0;
        screen->draw_offsets[0].y = thread->y + g_equip_gfx_draw_offset_y;
        SetDrawOffset(&screen->draw_offsets[0], &screen->draw_offsets[0].x);
        screen->draw_offsets[1].x = -0x80;
        screen->draw_offsets[1].y = g_equip_gfx_draw_offset_y;
        SetDrawOffset(&screen->draw_offsets[1], &screen->draw_offsets[1].x);
        offset[0] = thread->x - 0x73;
        offset[1] = thread->y;
        equip_gfx_build_scaled_draw_area_packets(&screen->portrait, &g_equip_character_status_frame_rect[4], frame,
            g_main_gfx_screen_polarity * 0xf0, (const s16*)offset);
        equip_panel_set_primitive_colors(screen, (const battle_menu_status_panel_frame_config_t*)thread);
        equip_gfx_apply_menu_palette_for_mode(&screen->numeric_frame, (s32*)thread);
        battle_gfx_draw_or_append_gpu_primitive(&screen->portrait.areas[1]);
        battle_gfx_draw_or_append_gpu_primitive(&screen->draw_offsets[1]);
        battle_gfx_draw_or_append_gpu_primitive(&screen->sprites[1]);
        for (i = 7; i < 12; i++) {
            battle_gfx_draw_or_append_gpu_primitive(&screen->sprites[i]);
        }
        battle_gfx_draw_or_append_gpu_primitive(&screen->draw_modes[0]);
        battle_gfx_draw_or_append_gpu_primitive(&screen->sprites[13]);
        battle_gfx_draw_or_append_gpu_primitive(&screen->draw_modes[1]);
        battle_gfx_draw_or_append_gpu_primitive(&screen->tiles[1]);
        for (i = 4; i < 8; i++) {
            battle_gfx_draw_or_append_gpu_primitive(&screen->lines[i]);
        }
        battle_menu_submit_numeric_display_frame_primitives(&screen->numeric_frame);
        battle_gfx_draw_or_append_gpu_primitive(&screen->draw_offsets[0]);
        battle_gfx_draw_or_append_gpu_primitive(&screen->portrait);
        battle_thread_yield();
        if (battle_thread_get_current_parameter_3() != 0) {
            break;
        }
        frame++;
    }
    battle_thread_yield();
    battle_thread_exit_current();
}
