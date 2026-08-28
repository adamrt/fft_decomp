#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/battle_text.h"
#include "fft/bunit.h"
#include "fft/main_heap.h"
#include "fft/menu.h"
#include "fft/menu_types.h"
#include "fft/status_panel.h"
#include "fft/thread.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* The packet, state and thread layouts are the shared fft/status_panel.h
 * records of the DEBUGCHR twin debugchr_render_unit_status_panel_thread. */

void battle_gfx_init_image_loading(POLY_FT4* primitive, const battle_image_location_t* base_load,
    const battle_image_location_t* base_screen, const world_gfx_image_load_parameters_t* parameters);
struct menu_number_entry;
struct menu_number_position;

/* Thread task 0x39: the character editor and status panels. BUNIT twin of
 * DEBUGCHR debugchr_render_unit_status_panel_thread. */
void bunit_render_unit_status_panel_thread(void) {
    RECT rects[6];
    s32 anim_state;
    s32 cur_unit;
    s32 prev_unit;

    u8* render_a;
    status_panel_editor_packet_t* editor_base;
    status_panel_numeric_entry_t* env_a;
    status_panel_editor_state_t* state;
    void* portrait_arg;
    u8* transition;
    u8* image;
    u8* render_b;
    u8* render_c;
    status_panel_packet_t* panel_base;
    s16* scroll;
    status_panel_display_thread_t* thread;
    s32 shake;
    s32 highlight;
    s32 suppress;

    s32 thread_id;
    u8* threads;
    s32 frame;
    u8* walk;
    u8* walk2;
    u8* walk3;
    s32 i;
    s32 off;
    s32 off1;
    s32 off2;
    s32 off3;
    s32 off4;
    s32* win;
    s32 px1;
    s32 pya;
    s32 px2;
    s32 pyb;
    s32 dx1;
    s32 dx2;
    s32 dx3;
    s32 dx4;
    s16 srow;
    s32* origin;
    status_panel_packet_t* panel;
    status_panel_editor_packet_t* editor;
    u8* cur;
    u8* cur1;
    u8* cur2;
    u8* cur3;
    u8* cur5;

    suppress = 0;
    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_UNIT_STATUS_BANNER);
    cur = (u8*)battle_script_get_controller_input_pointer(0);
    thread_id = g_battle_current_thread_id;
    threads = (u8*)g_battle_threads;
    g_bunit_input_controller = (u32*)(cur + 4);
    thread = *(status_panel_display_thread_t**)((thread_id * NATIVE_THREAD_STRIDE) + (u32)threads);
    if (thread_id == 8) {
        panel = g_bunit_panel_selected_packets;
        editor = g_bunit_panel_selected_editor_packets;
        render_a = g_bunit_panel_selected_number_image;
        render_b = g_bunit_panel_selected_large_number_image;
        render_c = g_bunit_panel_selected_name_image;
        env_a = g_bunit_panel_selected_numeric_entries;
        state = &g_bunit_panel_selected_billboard;
        scroll = g_bunit_panel_selected_unit_data;
        transition = g_bunit_panel_selected_portrait_rect;
        panel_base = panel;
        editor_base = editor;
        image = g_bunit_panel_selected_portrait_image;
    } else {
        panel = g_bunit_panel_comparison_packets;
        editor = g_bunit_panel_comparison_editor_packets;
        render_a = g_bunit_panel_comparison_number_image;
        render_b = g_bunit_panel_comparison_large_number_image;
        render_c = g_bunit_panel_comparison_name_image;
        env_a = g_bunit_panel_comparison_numeric_entries;
        state = &g_bunit_panel_comparison_billboard;
        scroll = g_bunit_panel_comparison_unit_data;
        transition = g_bunit_panel_comparison_portrait_rect;
        panel_base = panel;
        editor_base = editor;
        image = g_bunit_panel_comparison_portrait_image;
    }
    shake = 0;

    battle_menu_init_numeric_display_frame_primitives(
        (RECT*)g_bunit_gfx_portrait_origin, (struct world_menu_palette_primitives*)editor);
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
    walk = g_bunit_panel_editor_value_cells;
    off2 = 0x104;
    do {
        battle_gfx_init_image_loading(
            (u8*)editor + off2, g_bunit_editor_numeric_geometry, g_bunit_gfx_portrait_origin, walk);
        walk += 0xC;
        frame += 1;
        off2 += 0x14;
    } while (frame < 4);
    if (state->mode >= 4) {
        state->mode = 0;
    }
    if (g_battle_post_battle_unit_changes_active == 0) {
        battle_copy_bytes(g_bunit_panel_editor_mode_cell, g_bunit_panel_editor_mode_cells + state->mode * 0xC, 0xC);
    } else {
        battle_copy_bytes(g_bunit_panel_editor_mode_cell, g_bunit_panel_editor_mode_cells, 0xC);
    }
    frame = 0;
    walk2 = g_bunit_panel_editor_label_cells;
    off3 = 0x154;
    do {
        battle_gfx_init_image_loading(
            (u8*)editor + off3, g_bunit_editor_numeric_geometry, g_bunit_gfx_portrait_origin, walk2);
        walk2 += 0xC;
        frame += 1;
        off3 += 0x14;
    } while (frame < 7);
    battle_copy_bytes(editor + 1, editor, sizeof(status_panel_editor_packet_t));
    battle_gfx_set_draw_mode_for_texture_page(&panel->draw_mode_a, 0);
    battle_gfx_set_draw_mode_for_texture_page(&panel->draw_mode_b, 1);
    battle_menu_init_numeric_display_frame_primitives(
        (RECT*)g_bunit_panel_frame_rect, (struct world_menu_palette_primitives*)panel);
    cur1 = (u8*)&panel->sprites[0];
    battle_menu_init_sprite_array(cur1, 7, 0x7C3C);
    battle_gfx_init_default_poly_ft4(&panel->portrait);
    battle_text_configure_sprite_vram(&rects[2], 0x58, 0x20, cur1, 0);
    battle_text_configure_sprite_vram(&rects[3], 0x10, 0xA, &panel->sprites[1], 0);
    battle_text_configure_sprite_vram(&rects[4], 0x10, 0xA, &panel->sprites[2], 0);
    battle_text_configure_sprite_vram(&rects[5], 0x10, 0xA, &panel->sprites[3], 0);
    frame = 0;
    walk3 = g_bunit_panel_sprite_cells;
    off4 = 0xEC;
    do {
        battle_gfx_init_image_loading(
            (u8*)panel + off4, g_bunit_editor_numeric_geometry, g_bunit_panel_frame_rect, walk3);
        walk3 += 0xC;
        frame += 1;
        off4 += 0x14;
    } while (frame < 7);
    battle_gfx_init_image_loading(
        &panel->portrait, g_bunit_editor_numeric_geometry, g_bunit_panel_frame_rect, g_bunit_panel_portrait_cell);
    if (state->mode == 1) {
        panel->portrait.clut = 0x7FFD;
    } else {
        panel->portrait.clut = 0x7FBD;
    }
    panel->portrait.tpage = GetTPage(0, 1, 0x3C0, 0x100);
    battle_copy_bytes(panel + 1, panel, sizeof(status_panel_packet_t));
    frame = 0;
    anim_state = 0;
    prev_unit = state->unit;
    cur_unit = state->unit;
    for (;;) {
        s32 parity = frame & 1;
        panel = panel_base + parity;
        editor = editor_base + parity;
        if (state->bars[2].value >= 0x65) {
            state->bars[2].value = 0x64;
        }
        if (g_battle_post_battle_unit_changes_active == 0) {
            battle_copy_bytes(g_bunit_panel_editor_mode_cell, g_bunit_panel_editor_mode_cells + state->mode * 0xC, 0xC);
        } else {
            battle_copy_bytes(g_bunit_panel_editor_mode_cell, g_bunit_panel_editor_mode_cells, 0xC);
        }
        battle_gfx_init_image_loading(&editor->label_sprites[6], g_bunit_editor_numeric_geometry,
            g_bunit_gfx_portrait_origin, g_bunit_panel_editor_mode_cell);
        if (state->mode == 1) {
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
        for (i = 0; i < 4; i++) {
            editor->value_sprites[i].clut = g_bunit_panel_editor_value_cluts[i][highlight];
        }
        for (i = 0; i < 7; i++) {
            editor->label_sprites[i].clut = g_bunit_panel_editor_label_cluts[i][highlight];
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
        g_bunit_gfx_draw_offset_y = (u16)g_bunit_frame_arg != 0xF0 ? 0xF0 : 0;
        if ((thread->flags & 0x20) && (shake == 0)) {
            shake = 1;
        }
        if (thread->flags & 0x40) {
            if (shake == 0) {
                shake = 0xB;
            }
        }
        if (shake != 0) {
            if (shake < 0xA) {
                thread->shake_y.word = g_bunit_panel_slide_down_y[shake];
                if (frame & 1) {
                    shake += 1;
                }
                if (shake >= 7) {
                    shake = 0;
                    thread->flags = thread->flags & 0x80;
                }
            } else if (shake >= 0xB) {
                thread->shake_y.word = g_bunit_panel_slide_up_y[shake];
                if (frame & 1) {
                    shake += 1;
                }
                if (shake >= 0x11) {
                    shake = 0;
                    thread->flags = thread->flags & 0x80;
                }
            }
        }
        px1 = thread->x;
        dx1 = (u16)g_bunit_gfx_draw_offset_y;
        panel->draw_offset_a.x = px1 - 0x80;
        pya = thread->shake_y.low;
        panel->draw_offset_a.y = pya + dx1;
        SetDrawOffset(&panel->draw_offset_a, &panel->draw_offset_a.x);
        dx2 = (u16)g_bunit_gfx_draw_offset_y;
        panel->draw_offset_b.x = -0x80;
        panel->draw_offset_b.y = dx2;
        SetDrawOffset(&panel->draw_offset_b, &panel->draw_offset_b.x);
        px2 = thread->x;
        dx3 = (u16)g_bunit_gfx_draw_offset_y;
        editor->draw_offset_a.x = px2 - 0x80;
        pyb = thread->shake_y.low;
        editor->draw_offset_a.y = pyb + dx3;
        SetDrawOffset(&editor->draw_offset_a, &editor->draw_offset_a.x);
        dx4 = (u16)g_bunit_gfx_draw_offset_y;
        editor->draw_offset_b.x = -0x80;
        editor->draw_offset_b.y = dx4;
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
                cur_unit = state->unit;
            } else {
                portrait_arg = *(void* volatile*)&thread->work;
                anim_state = 2;
                prev_unit = cur_unit;
                cur_unit = state->unit;
            }
        }
        if (cur_unit != prev_unit) {
            if (anim_state == 0) {
                anim_state = 2;
            }
        }
        bunit_gfx_build_portrait_transition_primitives((const RECT*)transition, &anim_state, &cur_unit, &prev_unit,
            image, (POLY_FT4*)&editor->portrait[0], (s32)portrait_arg);
        {
            CVECTOR* color;
            u16* py;
            s32 scale;
            s32 arg;
            status_panel_bar_t* rec;
            volatile status_panel_editor_packet_t* dst;
            color = g_bunit_panel_gauge_bar_colors;
            i = 0;
            /* The bar rows read the origin y with lhu; an s16 walk changes the loads. */
            py = (u16*)&g_bunit_gfx_portrait_origin[1];
            scale = 0x18;
            dst = editor;
            arg = 0x1E0;
            rec = &state->bars[0];
            do {
                s32 den;
                s32 py0, py1, py2, py3;
                /* Zero-instruction bindings: left to the allocator, the two
                   read-backs and the last y load rotate one register (v1/a0/v0
                   instead of v0/v1/a0). The DEBUGCHR twin needs the same bindings. */
                register s32 x3_read __asm__("$3");
                register s32 x2_read __asm__("$2");
                u16 x0;
                s32 w;
                SetPolyG4((u8*)editor + arg);
                den = rec->limit;
                {
                    s32 origin_x;
                    origin_x = g_bunit_gfx_portrait_origin[0];
                    x0 = origin_x + 0x2F;
                }
                if (den == 0) {
                    w = 4;
                } else {
                    w = (rec->value << 5) / den;
                }
                if (w == 0) {
                    x0 = 0;
                }
                dst->bars[0].x0 = x0;
                py0 = *py;
                dst->bars[0].x1 = x0 + w;
                dst->bars[0].y0 = py0 + scale;
                py1 = *py;
                dst->bars[0].x2 = x0;
                dst->bars[0].y1 = py1 + scale;
                py2 = *py;
                dst->bars[0].x3 = x0 + w;
                x3_read = (u16)dst->bars[0].x3;
                dst->bars[0].y2 = py2 + scale + 3;
                py3 = *py;
                x2_read = (u16)dst->bars[0].x2;
                dst->bars[0].x3 = x3_read - 3;
                dst->bars[0].x2 = x2_read - 3;
                dst->bars[0].y3 = py3 + scale + 3;
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
                rec++;
                scale += 0xB;
                dst = (volatile status_panel_editor_packet_t*)((u8*)dst + sizeof(POLY_G4));
                i += 1;
                arg += 0x24;
            } while (i < 3);
        }
        panel->sprites[6].u0 = (s16)(scroll[4] % 7) * 0x18;
        srow = scroll[4];
        panel->sprites[6].w = 0x18;
        panel->sprites[6].h = 0x14;
        panel->sprites[6].v0 = (s16)(srow / 7) * 0x14 + 0x2A;
        if ((frame == 0) || (thread->work != 0)) {
            if (state->list_index < 0) {
                env_a[6].format = 0xC00;
            } else {
                env_a[6].format = 2;
            }
            battle_clear_menu_render_buffer(render_a, 0x700);
            win = &g_menu_inner_window_width;
            origin = win - 2;
            *win = 0x38;
            battle_menu_draw_numeric_display_entries(
                (s32)render_a, (struct menu_number_entry*)env_a, (struct menu_number_position*)origin, 6);
            LoadImage(&rects[0], (u32*)render_a);
            cur2 = render_a + 0x400;
            *win = 0x60;
            battle_menu_draw_numeric_display_entries(
                (s32)cur2, (struct menu_number_entry*)&env_a[6], (struct menu_number_position*)origin, 4);
            LoadImage(&rects[1], (u32*)cur2);
            thread->work = 0;
            battle_clear_menu_render_buffer(render_c, 0x580);
            *win = 0x58;
            battle_menu_set_text_origin(0, 0);
            battle_menu_display_text_entry(scroll[0] + 0x4000, render_c, origin);
            battle_menu_set_text_origin(0, 0x10);
            battle_menu_display_text_entry(scroll[1] + 0x3000, render_c, origin);
            LoadImage(&rects[2], (u32*)render_c);
            battle_clear_menu_render_buffer(render_b, 0xF0);
            *win = 0x10;
            battle_menu_set_text_origin(0, 0);
            battle_text_draw_large_number_glyphs(scroll[2], 0x202, render_b, origin);
            LoadImage(&rects[3], (u32*)render_b);
            battle_menu_set_text_origin(0, 0);
            cur3 = render_b + 0x50;
            battle_text_draw_large_number_glyphs(scroll[3], 0x202, cur3, origin);
            LoadImage(&rects[4], (u32*)cur3);
            if (g_battle_post_battle_unit_changes_active == 0) {
                battle_stats_t* stats;
                battle_menu_set_text_origin(0, 0);
                stats = battle_unit_get_stats_from_battle_id(scroll[5]);
                if (scroll[5] >= 0x10) {
                    s32 id = stats->formation_index;
                    if (id == 0xFF) {
                        id = 0;
                    }
                    battle_text_draw_large_number_glyphs(id + 1, 0x4002, render_b + 0xA0, origin);
                }
            }
            LoadImage(&rects[5], (u32*)(render_b + 0xA0));
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
            cur5 = (u8*)&panel->portrait;
            SetSemiTrans(cur5, 1);
            battle_gfx_draw_or_append_gpu_primitive(cur5);
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
        frame++;
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
