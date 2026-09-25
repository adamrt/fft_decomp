/* REQUIRE twin of debugchr_render_unit_status_panel_thread: the same doubled
 * 0x30C editor / 0x1D8 panel packets (fft/battle_menu_status_panel.h), without the shake
 * offsets, the bar division and the zodiac branch.
 *
 * Compiler constraints (no instructions emitted): the two empty asm operands
 * in the clut loops keep the table offset out of loop.c's induction variables
 * (as in the twin), and the bar loop pins the two volatile read-backs to $2/$3,
 * which the allocator otherwise rotates by one register.
 */
#include "fft/battle_gfx.h"
#include "fft/battle_text.h"
#include "fft/event.h"
#include "fft/main_gfx.h"
#include "fft/main_heap.h"
#include "fft/menu.h"
#include "fft/menu_types.h"
#include "fft/require.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/types.h"

struct world_menu_palette_primitives;
void battle_gfx_init_image_loading(POLY_FT4* primitive, const battle_image_location_t* base_load,
    const battle_image_location_t* base_screen, const world_gfx_image_load_parameters_t* parameters);
struct menu_number_entry;
struct menu_number_position;

void require_render_unit_status_panel_thread(void) {
    RECT rects[6];
    s32 anim_state;
    s32 cur_unit;
    s32 prev_unit;

    u8* number_image;
    battle_menu_status_panel_editor_packet_t* editor_base;
    battle_menu_status_panel_numeric_entry_t* numeric_entries;
    battle_menu_status_panel_gauges_t* state;
    void* portrait_arg;
    u8* portrait_rect;
    u8* portrait_image;
    u8* small_text_image;
    u8* name_image;
    battle_menu_status_panel_packet_t* panel_base;
    s16* unit_data;
    battle_menu_status_panel_display_thread_t* thread;
    s32 frame;
    s32 highlight;
    s32 suppress;

    s32 thread_id;
    u8* threads;
    u8* walk;
    u8* walk2;
    u8* walk3;
    s32 i;
    s32 off;
    s32 off1;
    s32 off2;
    s32 off3;
    s32 off4;
    s32* window_width;
    s32 panel_x;
    s32 editor_x;
    s16 icon_index;
    s32* text_position;
    battle_menu_status_panel_packet_t* panel;
    battle_menu_status_panel_editor_packet_t* editor;
    u8* input_state;
    SPRT* name_sprite;
    u8* number_image_lower;
    u8* small_text_image_2;
    u8* small_text_image_3;
    u8* portrait_poly;

    suppress = 0;
    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_UNIT_STATUS_BANNER);
    input_state = (u8*)battle_script_get_controller_input_pointer(0);
    thread_id = g_battle_current_thread_id;
    threads = (u8*)g_battle_threads;
    g_require_input_controller = (u32*)(input_state + 4);
    thread = *(battle_menu_status_panel_display_thread_t**)((thread_id * NATIVE_THREAD_STRIDE) + (u32)threads);
    if (thread_id == 8) {
        panel = g_require_panel_selected_packets;
        editor = g_require_panel_selected_editor_packets;
        number_image = g_require_panel_selected_number_image;
        small_text_image = g_require_panel_selected_large_number_image;
        name_image = g_require_panel_selected_name_image;
        numeric_entries = g_require_panel_selected_numeric_entries;
        state = &g_require_panel_selected_billboard;
        unit_data = (s16*)g_require_active_unit_data;
        portrait_rect = g_require_panel_selected_portrait_rect;
        panel_base = panel;
        editor_base = editor;
        portrait_image = g_require_panel_selected_portrait_image;
    } else {
        panel = g_require_panel_comparison_packets;
        editor = g_require_panel_comparison_editor_packets;
        number_image = g_require_panel_comparison_number_image;
        small_text_image = g_require_panel_comparison_large_number_image;
        name_image = g_require_panel_comparison_name_image;
        numeric_entries = g_require_panel_comparison_numeric_entries;
        state = &g_require_panel_comparison_billboard;
        unit_data = g_require_panel_comparison_unit_data;
        portrait_rect = g_require_panel_comparison_portrait_rect;
        panel_base = panel;
        editor_base = editor;
        portrait_image = g_require_panel_comparison_portrait_image;
    }

    battle_menu_init_numeric_display_frame_primitives(
        (RECT*)g_require_gfx_portrait_origin, (struct world_menu_palette_primitives*)editor);
    battle_gfx_set_draw_mode_for_texture_page(&editor->draw_mode_a, 1);
    battle_gfx_set_draw_mode_for_texture_page(&editor->draw_mode_b, 0);
    battle_menu_init_sprite_array(&editor->label_sprites[0], 7, 0x7CBC);
    battle_menu_init_sprite_array(&editor->value_sprites[0], 4, 0x7CBC);
    frame = 0;
    off1 = 0x24C;
    do {
        battle_gfx_init_default_poly_ft4((POLY_FT4*)((u8*)editor + off1));
        frame += 1;
        off1 += 0x28;
    } while (frame < 4);
    battle_text_configure_sprite_vram(&rects[0], 0x38, 0x28, &editor->value_sprites[0], 2);
    battle_text_configure_sprite_vram(&rects[1], 0x60, 0x10, &editor->value_sprites[1], 2);
    frame = 0;
    walk = g_require_panel_editor_value_cells;
    off2 = 0x104;
    do {
        battle_gfx_init_image_loading(
            (u8*)editor + off2, g_require_editor_numeric_geometry, g_require_gfx_portrait_origin, walk);
        walk += 0xC;
        frame += 1;
        off2 += 0x14;
    } while (frame < 4);
    if (state->team_state >= 4) {
        state->team_state = 0;
    }
    if (g_battle_post_battle_unit_changes_active == 0) {
        battle_copy_bytes(
            g_require_panel_editor_mode_cell, g_require_panel_editor_mode_cells + state->team_state * 0xC, 0xC);
    } else {
        battle_copy_bytes(g_require_panel_editor_mode_cell, g_require_panel_editor_mode_cells, 0xC);
    }
    frame = 0;
    walk2 = g_require_panel_editor_label_cells;
    off3 = 0x154;
    do {
        battle_gfx_init_image_loading(
            (u8*)editor + off3, g_require_editor_numeric_geometry, g_require_gfx_portrait_origin, walk2);
        walk2 += 0xC;
        frame += 1;
        off3 += 0x14;
    } while (frame < 7);
    battle_copy_bytes(editor + 1, editor, sizeof(battle_menu_status_panel_editor_packet_t));
    battle_gfx_set_draw_mode_for_texture_page(&panel->draw_mode_a, 0);
    battle_gfx_set_draw_mode_for_texture_page(&panel->draw_mode_b, 1);
    battle_menu_init_numeric_display_frame_primitives(
        (RECT*)g_require_panel_frame_rect, (struct world_menu_palette_primitives*)panel);
    name_sprite = &panel->sprites[0];
    battle_menu_init_sprite_array(name_sprite, 7, 0x7C3C);
    battle_gfx_init_default_poly_ft4(&panel->portrait);
    battle_text_configure_sprite_vram(&rects[2], 0x58, 0x20, name_sprite, 0);
    battle_text_configure_sprite_vram(&rects[3], 0x10, 0xA, &panel->sprites[1], 0);
    battle_text_configure_sprite_vram(&rects[4], 0x10, 0xA, &panel->sprites[2], 0);
    battle_text_configure_sprite_vram(&rects[5], 0x10, 0xA, &panel->sprites[3], 0);
    frame = 0;
    walk3 = g_require_panel_sprite_cells;
    off4 = 0xEC;
    do {
        battle_gfx_init_image_loading(
            (u8*)panel + off4, g_require_editor_numeric_geometry, g_require_panel_frame_rect, walk3);
        walk3 += 0xC;
        frame += 1;
        off4 += 0x14;
    } while (frame < 7);
    battle_gfx_init_image_loading(
        &panel->portrait, g_require_editor_numeric_geometry, g_require_panel_frame_rect, g_require_panel_portrait_cell);
    if (state->team_state == 1) {
        panel->portrait.clut = 0x7FFD;
    } else {
        panel->portrait.clut = 0x7FBD;
    }
    panel->portrait.tpage = GetTPage(0, 1, 0x3C0, 0x100);
    battle_copy_bytes(panel + 1, panel, sizeof(battle_menu_status_panel_packet_t));
    frame = 0;
    anim_state = 0;
    prev_unit = state->unit_index;
    cur_unit = state->unit_index;
    for (;;) {
        s32 parity = frame & 1;
        panel = panel_base + parity;
        editor = editor_base + parity;
        if (state->ct >= 0x65) {
            state->ct = 0x64;
        }
        if (g_battle_post_battle_unit_changes_active == 0) {
            battle_copy_bytes(
                g_require_panel_editor_mode_cell, g_require_panel_editor_mode_cells + state->team_state * 0xC, 0xC);
        } else {
            battle_copy_bytes(g_require_panel_editor_mode_cell, g_require_panel_editor_mode_cells, 0xC);
        }
        battle_gfx_init_image_loading(&editor->label_sprites[6], g_require_editor_numeric_geometry,
            g_require_gfx_portrait_origin, g_require_panel_editor_mode_cell);
        if (state->team_state == 1) {
            panel->portrait.clut = 0x7FFD;
        } else {
            panel->portrait.clut = 0x7FBD;
        }
        highlight = 0;
        if (battle_thread_find_running_by_task(NATIVE_THREAD_TASK_STATUS_PANEL) != 0) {
            highlight = g_event_mode != 0;
        }
        if (thread->highlight != 0) {
            highlight = 1;
        }
        i = 0;
        {
            u16* clut_table;
            battle_menu_status_panel_editor_packet_t* dst;
            s32 clut_offset;
            clut_offset = highlight * 2;
            clut_table = g_require_panel_editor_value_cluts;
            dst = editor;
            do {
                /* Keeps the offset loop-variant; otherwise loop.c folds it into
                   the table pointer and the read loses its separate addu. */
                __asm__("" : "=r"(clut_offset) : "0"(clut_offset));
                dst->value_sprites[0].clut = *(u16*)(clut_offset + (u32)clut_table);
                clut_table += 2;
                i += 1;
                dst = (battle_menu_status_panel_editor_packet_t*)((u8*)dst + sizeof(SPRT));
            } while (i < 4);
        }
        i = 0;
        {
            u16* clut_table;
            battle_menu_status_panel_editor_packet_t* dst;
            s32 clut_offset;
            clut_offset = highlight * 2;
            clut_table = g_require_panel_editor_label_cluts;
            dst = editor;
            do {
                /* Same loop-variant offset as the loop above. */
                __asm__("" : "=r"(clut_offset) : "0"(clut_offset));
                dst->label_sprites[0].clut = *(u16*)(clut_offset + (u32)clut_table);
                clut_table += 2;
                i += 1;
                dst = (battle_menu_status_panel_editor_packet_t*)((u8*)dst + sizeof(SPRT));
            } while (i < 7);
        }
        i = 0;
        do {
            if (highlight != 0) {
                panel->sprites[i].clut = 0x7D3C;
            } else {
                panel->sprites[i].clut = 0x7C3C;
            }
            i += 1;
        } while (i < 7);
        if (highlight != 0) {
            battle_menu_init_primitive_colors_palette_bank_1((struct world_menu_palette_primitives*)editor);
            battle_menu_init_primitive_colors_palette_bank_1((struct world_menu_palette_primitives*)panel);
            if ((u32)(anim_state - 4) < 6U) {
                editor->portrait[0].r0 = 0x20;
                editor->portrait[0].g0 = 0x28;
                editor->portrait[0].b0 = 0x38;
            } else {
                editor->portrait[0].r0 = 0x40;
                editor->portrait[0].g0 = 0x50;
                editor->portrait[0].b0 = 0x70;
            }
            editor->portrait[1].r0 = 0x20;
            editor->portrait[1].g0 = 0x28;
            editor->portrait[1].b0 = 0x38;
            editor->portrait[2].r0 = 0x20;
            editor->portrait[2].g0 = 0x28;
            editor->portrait[2].b0 = 0x38;
            editor->portrait[3].r0 = 0x20;
            editor->portrait[3].g0 = 0x28;
            editor->portrait[3].b0 = 0x38;
        } else {
            s32 shade;
            battle_menu_init_primitive_colors_palette_bank_0((struct world_menu_palette_primitives*)editor);
            battle_menu_init_primitive_colors_palette_bank_0((struct world_menu_palette_primitives*)panel);
            if ((u32)(anim_state - 4) >= 6U) {
                editor->portrait[0].r0 = 0x80;
                editor->portrait[0].g0 = 0x80;
                editor->portrait[0].b0 = 0x80;
            } else {
                editor->portrait[0].r0 = 0x40;
                editor->portrait[0].g0 = 0x40;
                editor->portrait[0].b0 = 0x40;
            }
            shade = 0x40;
            editor->portrait[1].r0 = shade;
            editor->portrait[1].g0 = shade;
            editor->portrait[1].b0 = shade;
            editor->portrait[2].r0 = shade;
            editor->portrait[2].g0 = shade;
            editor->portrait[2].b0 = shade;
            editor->portrait[3].r0 = shade;
            editor->portrait[3].g0 = shade;
            editor->portrait[3].b0 = shade;
        }
        if (highlight != 0) {
            panel->portrait.r0 = 0x40;
            panel->portrait.g0 = 0x40;
            panel->portrait.b0 = 0x60;
        } else {
            panel->portrait.r0 = 0x80;
            panel->portrait.g0 = 0x80;
            panel->portrait.b0 = 0x80;
        }
        panel_x = thread->x;
        panel->draw_offset_a.x = panel_x - 0x80;
        g_require_gfx_draw_offset_y = ((g_main_gfx_screen_polarity * 0xF0) != 0xF0) ? 0xF0 : 0;
        panel->draw_offset_a.y = thread->shake_y.low + g_require_gfx_draw_offset_y;
        SetDrawOffset(&panel->draw_offset_a, &panel->draw_offset_a.x);
        panel->draw_offset_b.x = -0x80;
        panel->draw_offset_b.y = g_require_gfx_draw_offset_y;
        SetDrawOffset(&panel->draw_offset_b, &panel->draw_offset_b.x);
        editor_x = thread->x;
        editor->draw_offset_a.x = editor_x - 0x80;
        editor->draw_offset_a.y = thread->shake_y.low + g_require_gfx_draw_offset_y;
        SetDrawOffset(&editor->draw_offset_a, &editor->draw_offset_a.x);
        editor->draw_offset_b.x = -0x80;
        editor->draw_offset_b.y = g_require_gfx_draw_offset_y;
        SetDrawOffset(&editor->draw_offset_b, &editor->draw_offset_b.x);
        if (anim_state >= 0xA) {
            anim_state = 0;
            prev_unit = cur_unit;
            suppress = thread->flags & 0x80;
        }
        if (thread->work != 0) {
            if (anim_state == 0) {
                portrait_arg = *(void* volatile*)&thread->work;
                prev_unit = cur_unit;
                cur_unit = state->unit_index;
            } else {
                portrait_arg = *(void* volatile*)&thread->work;
                anim_state = 2;
                prev_unit = cur_unit;
                cur_unit = state->unit_index;
            }
        }
        if (cur_unit != prev_unit) {
            if (anim_state == 0) {
                anim_state = 2;
            }
        }
        require_gfx_build_portrait_transition_primitives((const RECT*)portrait_rect, &anim_state, &cur_unit, &prev_unit,
            (u8*)portrait_image, (POLY_FT4*)&editor->portrait[0], (s32)portrait_arg);
        {
            CVECTOR* color;
            s16* origin_xy;
            s32 row_offset;
            s32 prim_offset;
            volatile battle_menu_status_panel_editor_packet_t* dst;
            color = g_require_panel_gauge_bar_colors;
            i = 0;
            origin_xy = g_require_gfx_portrait_origin;
            row_offset = 0x18;
            dst = editor;
            prim_offset = 0x1E0;
            do {
                s32 x0;
                s32 x1;
                s32 py0, py1, py2;
                s32 py3;
                register s32 x2_read __asm__("$2");
                register s32 x3_read __asm__("$3");
                SetPolyG4((u8*)editor + prim_offset);
                x1 = origin_xy[0];
                x0 = x1 + 0x2F;
                dst->bars[0].x0 = x0;
                py0 = (u16)origin_xy[1];
                x1 += 0x4F;
                dst->bars[0].x1 = x1;
                dst->bars[0].y0 = py0 + row_offset;
                py1 = (u16)origin_xy[1];
                dst->bars[0].x2 = x0;
                dst->bars[0].y1 = py1 + row_offset;
                py2 = (u16)origin_xy[1];
                dst->bars[0].x3 = x1;
                x3_read = (u16)dst->bars[0].x3;
                dst->bars[0].y2 = py2 + row_offset + 3;
                py3 = (u16)origin_xy[1];
                x2_read = (u16)dst->bars[0].x2;
                dst->bars[0].x3 = x3_read - 3;
                dst->bars[0].x2 = x2_read - 3;
                dst->bars[0].y3 = py3 + row_offset + 3;
                if (highlight != 0) {
                    dst->bars[0].r0 = color->r >> 1;
                    dst->bars[0].g0 = color->g >> 1;
                    dst->bars[0].b0 = color->b;
                    color++;
                    dst->bars[0].r1 = color->r >> 1;
                    dst->bars[0].g1 = color->g >> 1;
                    dst->bars[0].b1 = color->b;
                    color++;
                    dst->bars[0].r2 = color->r >> 1;
                    dst->bars[0].g2 = color->g >> 1;
                    dst->bars[0].b2 = color->b;
                    color++;
                    dst->bars[0].r3 = color->r >> 1;
                    dst->bars[0].g3 = color->g >> 1;
                    dst->bars[0].b3 = color->b;
                    color++;
                } else {
                    dst->bars[0].r0 = color->r;
                    dst->bars[0].g0 = color->g;
                    dst->bars[0].b0 = color->b;
                    color++;
                    dst->bars[0].r1 = color->r;
                    dst->bars[0].g1 = color->g;
                    dst->bars[0].b1 = color->b;
                    color++;
                    dst->bars[0].r2 = color->r;
                    dst->bars[0].g2 = color->g;
                    dst->bars[0].b2 = color->b;
                    color++;
                    dst->bars[0].r3 = color->r;
                    dst->bars[0].g3 = color->g;
                    dst->bars[0].b3 = color->b;
                    color++;
                }
                row_offset += 0xB;
                dst = (volatile battle_menu_status_panel_editor_packet_t*)((u8*)dst + sizeof(POLY_G4));
                i += 1;
                prim_offset += 0x24;
            } while (i < 3);
        }
        panel->sprites[6].u0 = (s16)(unit_data[4] % 7) * 0x18;
        icon_index = unit_data[4];
        panel->sprites[6].w = 0x18;
        panel->sprites[6].h = 0x14;
        panel->sprites[6].v0 = (s16)(icon_index / 7) * 0x14 + 0x2A;
        if ((frame == 0) || (thread->work != 0)) {
            if (state->_04 < 0) {
                numeric_entries[6].format = 0xC00;
            } else {
                numeric_entries[6].format = 2;
            }
            battle_clear_menu_render_buffer(number_image, 0x700);
            window_width = &g_menu_inner_window_width;
            text_position = window_width - 2;
            *window_width = 0x38;
            battle_menu_draw_numeric_display_entries((s32)number_image, (struct menu_number_entry*)numeric_entries,
                (struct menu_number_position*)text_position, 6);
            LoadImage(&rects[0], (u32*)number_image);
            number_image_lower = number_image + 0x400;
            *window_width = 0x60;
            battle_menu_draw_numeric_display_entries((s32)number_image_lower,
                (struct menu_number_entry*)&numeric_entries[8], (struct menu_number_position*)text_position, 2);
            LoadImage(&rects[1], (u32*)number_image_lower);
            thread->work = 0;
            battle_clear_menu_render_buffer(name_image, 0x580);
            *window_width = 0x58;
            battle_menu_set_text_origin(0, 0);
            battle_menu_display_text_entry(unit_data[0] + TEXT_ID_UNIT_NAME_BASE, name_image, text_position);
            battle_menu_set_text_origin(0, 0x10);
            battle_menu_display_text_entry(unit_data[1] + TEXT_ID_JOB_NAME_BASE, name_image, text_position);
            LoadImage(&rects[2], (u32*)name_image);
            battle_clear_menu_render_buffer(small_text_image, 0xF0);
            *window_width = 0x10;
            battle_menu_set_text_origin(0, 0);
            battle_text_draw_large_number_glyphs(unit_data[2], 0x202, small_text_image, text_position);
            LoadImage(&rects[3], (u32*)small_text_image);
            battle_menu_set_text_origin(0, 0);
            small_text_image_2 = small_text_image + 0x50;
            battle_text_draw_large_number_glyphs(unit_data[3], 0x202, small_text_image_2, text_position);
            LoadImage(&rects[4], (u32*)small_text_image_2);
            if (g_battle_post_battle_unit_changes_active == 0) {
                battle_menu_set_text_origin(0, 0);
                small_text_image_3 = small_text_image + 0xA0;
                battle_text_draw_large_number_glyphs(unit_data[5] + 1, 0x4002, small_text_image_3, text_position);
                LoadImage(&rects[5], (u32*)small_text_image_3);
            } else {
                LoadImage(&rects[5], (u32*)(small_text_image + 0xA0));
            }
        }
        if (!(thread->flags & 0x80)) {
            battle_gfx_draw_or_append_gpu_primitive(&editor->draw_offset_b);
            battle_gfx_draw_or_append_gpu_primitive(&editor->value_sprites[0]);
            battle_gfx_draw_or_append_gpu_primitive(&editor->value_sprites[1]);
            battle_gfx_draw_or_append_gpu_primitive(&editor->draw_mode_a);
            i = 0;
            off = 0x1E0;
            do {
                battle_gfx_draw_or_append_gpu_primitive((u8*)editor + off);
                i += 1;
                off += 0x24;
            } while (i < 3);
            battle_gfx_draw_or_append_gpu_primitive(&editor->value_sprites[2]);
            battle_gfx_draw_or_append_gpu_primitive(&editor->value_sprites[3]);
            i = 0;
            off = 0x154;
            do {
                battle_gfx_draw_or_append_gpu_primitive((u8*)editor + off);
                i += 1;
                off += 0x14;
            } while (i < 7);
            battle_gfx_draw_or_append_gpu_primitive(&editor->draw_mode_b);
            if (suppress == 0) {
                if (anim_state >= 5) {
                    battle_gfx_draw_or_append_gpu_primitive(&editor->portrait[2]);
                    battle_gfx_draw_or_append_gpu_primitive(&editor->portrait[3]);
                    battle_gfx_draw_or_append_gpu_primitive(&editor->portrait[1]);
                }
                battle_gfx_draw_or_append_gpu_primitive(&editor->portrait[0]);
            }
            battle_menu_submit_numeric_display_frame_primitives(editor);
            battle_gfx_draw_or_append_gpu_primitive(&editor->draw_offset_a);
            battle_gfx_draw_or_append_gpu_primitive(&panel->draw_offset_b);
            portrait_poly = (u8*)&panel->portrait;
            SetSemiTrans(portrait_poly, 1);
            battle_gfx_draw_or_append_gpu_primitive(portrait_poly);
            i = 3;
            off = 0x128;
            do {
                battle_gfx_draw_or_append_gpu_primitive((u8*)panel + off);
                i -= 1;
                off -= 0x14;
            } while (i >= 0);
            battle_gfx_draw_or_append_gpu_primitive(&panel->draw_mode_b);
            i = 4;
            off = 0x13C;
            do {
                battle_gfx_draw_or_append_gpu_primitive((u8*)panel + off);
                i += 1;
                off += 0x14;
            } while (i < 7);
            battle_gfx_draw_or_append_gpu_primitive(&panel->draw_mode_a);
            battle_menu_submit_numeric_display_frame_primitives(panel);
            battle_gfx_draw_or_append_gpu_primitive(&panel->draw_offset_a);
        }
        battle_thread_yield();
        if (battle_thread_get_current_parameter_3() != 0) {
            break;
        }
        frame += 1;
    }
    battle_thread_yield();
    battle_gfx_free_tpage7_vram(&rects[0]);
    battle_gfx_free_tpage7_vram(&rects[1]);
    battle_gfx_free_tpage7_vram(&rects[2]);
    battle_gfx_free_tpage7_vram(&rects[3]);
    battle_gfx_free_tpage7_vram(&rects[4]);
    battle_gfx_free_tpage7_vram(&rects[5]);
    battle_thread_exit_current();
}
