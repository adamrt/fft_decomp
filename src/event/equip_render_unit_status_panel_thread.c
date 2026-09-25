#include "fft/event_equip.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* The packet, state and thread layouts are the shared fft/menu.h
 * records: the editor packets at g_equip_panel_selected_editor_packets / g_equip_panel_comparison_editor_packets, the
 * status-panel packets at g_equip_panel_selected_packets / g_equip_panel_comparison_packets, the editor states
 * g_equip_selected_unit_stat_summary / g_equip_panel_comparison_billboard, and the per-thread display record.
 * g_equip_panel_gauge_bar_colors holds the four Gouraud vertex colours of each bar. */

/* The bar loop advances its destination by one POLY_G4 per iteration instead of
   indexing bars[i], so every store keeps the target's positive 0x1E4-0x202
   displacement from an unmoved base; see the note at the loop. */

struct world_menu_palette_primitives;
struct menu_number_entry;
struct menu_number_position;

/*
 * EQUIP twin of debugchr_render_unit_status_panel_thread (DEBUGCHR
 * 0x801bf750): the same unit editor/status display thread, with thread slot
 * 0xD selecting the first packet set. When g_equip_item_numeric_thread_enabled is set, the first
 * numeric block is drawn as five separate entries (two through
 * equip_text_render_decimal_entry_list), as in world_menu_unit_status_banner_thread; the
 * zodiac/g_debugchr_editor_active branch of the DEBUGCHR version is absent.
 *
 * The register pins and empty asm barriers are inherited from the DEBUGCHR
 * twin and emit no instructions; see the notes at each use.
 */
void equip_render_unit_status_panel_thread(void) {
    RECT rects[6];
    s32 anim_state;
    s32 cur_unit;
    s32 prev_unit;

    u8* number_image;                                          /* sp60 */
    battle_menu_status_panel_editor_packet_t* editor_base;     /* sp68 */
    battle_menu_status_panel_numeric_entry_t* numeric_entries; /* sp70 */
    battle_menu_status_panel_editor_state_t* state;            /* sp78 */
    void* portrait_arg;                                        /* sp80 */
    const RECT* portrait_rect;                                 /* sp88 */
    u8* portrait_image;                                        /* sp90 */
    u8* small_text_image;                                      /* sp98 */
    u8* name_image;                                            /* spA0 */
    battle_menu_status_panel_packet_t* panel_base;             /* spA8 */
    s16* unit_data;                                            /* spB0 */
    battle_menu_status_panel_display_thread_t* thread;         /* spB8 */
    s32 shake;                                                 /* spC0 */
    s32 highlight;                                             /* spC8 */
    s32 suppress;                                              /* spD0 */

    s32 thread_id;
    s32 done;
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
    s32 panel_x;
    s32 panel_shake_y;
    s32 editor_x;
    s32 editor_shake_y;
    s32* window_width;
    s32 panel_offset_y;
    s32 panel_base_offset_y;
    s32 editor_offset_y;
    s32 editor_base_offset_y;
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
    g_equip_input_controller = (u32*)(input_state + 4);
    /* Raw index arithmetic: g_battle_threads holds one pointer per 0x400-byte
       thread slot. The equivalent `threads + (thread_id << 10)` pointer form
       moves the shift and changes the prologue. */
    thread = *(battle_menu_status_panel_display_thread_t**)((thread_id * NATIVE_THREAD_STRIDE) + (u32)threads);
    if (thread_id == 0xD) {
        panel = g_equip_panel_selected_packets;
        editor = g_equip_panel_selected_editor_packets;
        number_image = g_equip_panel_selected_number_image;
        small_text_image = g_equip_panel_selected_large_number_image;
        name_image = g_equip_panel_selected_name_image;
        numeric_entries = g_equip_panel_selected_numeric_entries;
        /* Declared elsewhere in EQUIP as the 0x28-byte world_item_stat_summary_t;
         * this function reads it as the first of two 0x24-spaced editor states. */
        state = (battle_menu_status_panel_editor_state_t*)&g_equip_selected_unit_stat_summary;
        unit_data = (s16*)g_equip_panel_selected_unit_data;
        portrait_rect = &g_equip_panel_selected_portrait_rect;
        panel_base = panel;
        editor_base = editor;
        portrait_image = g_equip_panel_selected_portrait_image;
    } else {
        panel = g_equip_panel_comparison_packets;
        editor = g_equip_panel_comparison_editor_packets;
        number_image = g_equip_panel_comparison_number_image;
        small_text_image = g_equip_panel_comparison_large_number_image;
        name_image = g_equip_panel_comparison_name_image;
        numeric_entries = g_equip_panel_comparison_numeric_entries;
        state = &g_equip_panel_comparison_billboard;
        unit_data = g_equip_panel_comparison_unit_data;
        portrait_rect = &g_equip_panel_comparison_portrait_rect;
        panel_base = panel;
        editor_base = editor;
        portrait_image = g_equip_panel_comparison_portrait_image;
    }
    shake = 0;

    battle_menu_init_numeric_display_frame_primitives(&g_equip_gfx_portrait_origin.rect, &editor->numeric_frame);
    battle_gfx_set_draw_mode_for_texture_page(&editor->draw_mode_a, 1);
    battle_gfx_set_draw_mode_for_texture_page(&editor->draw_mode_b, 0);
    battle_menu_init_sprite_array(&editor->label_sprites[0], 7, 0x7CBC);
    battle_menu_init_sprite_array(&editor->value_sprites[0], 4, 0x7CBC);
    frame = 0;
    off1 = 0x24C;
    do {
        battle_gfx_init_default_poly_ft4((u8*)editor + off1);
        frame += 1;
        off1 += 0x28;
    } while (frame < 4);
    battle_text_configure_sprite_vram(&rects[0], 0x38, 0x28, &editor->value_sprites[0], 2);
    battle_text_configure_sprite_vram(&rects[1], 0x60, 0x10, &editor->value_sprites[1], 2);
    frame = 0;
    walk = g_equip_panel_editor_value_cells;
    off2 = 0x104;
    do {
        battle_gfx_init_image_loading(
            (u8*)editor + off2, g_equip_editor_numeric_geometry, &g_equip_gfx_portrait_origin.location, walk);
        walk += 0xC;
        frame += 1;
        off2 += 0x14;
    } while (frame < 4);
    if (state->mode >= 4) {
        state->mode = 0;
    }
    if (g_battle_post_battle_unit_changes_active == 0) {
        battle_copy_bytes(g_equip_panel_editor_mode_cell, g_equip_panel_editor_mode_cells + state->mode * 0xC, 0xC);
    } else {
        battle_copy_bytes(g_equip_panel_editor_mode_cell, g_equip_panel_editor_mode_cells, 0xC);
    }
    frame = 0;
    walk2 = g_equip_panel_editor_label_cells;
    off3 = 0x154;
    do {
        battle_gfx_init_image_loading(
            (u8*)editor + off3, g_equip_editor_numeric_geometry, &g_equip_gfx_portrait_origin.location, walk2);
        walk2 += 0xC;
        frame += 1;
        off3 += 0x14;
    } while (frame < 7);
    battle_copy_bytes(editor + 1, editor, sizeof(battle_menu_status_panel_editor_packet_t));
    battle_gfx_set_draw_mode_for_texture_page(&panel->draw_mode_a, 0);
    battle_gfx_set_draw_mode_for_texture_page(&panel->draw_mode_b, 1);
    battle_menu_init_numeric_display_frame_primitives(&g_equip_panel_frame_rect.rect, &panel->numeric_frame);
    name_sprite = &panel->sprites[0];
    battle_menu_init_sprite_array(name_sprite, 7, 0x7C3C);
    battle_gfx_init_default_poly_ft4(&panel->portrait);
    battle_text_configure_sprite_vram(&rects[2], 0x58, 0x20, name_sprite, 0);
    battle_text_configure_sprite_vram(&rects[3], 0x10, 0xA, &panel->sprites[1], 0);
    battle_text_configure_sprite_vram(&rects[4], 0x10, 0xA, &panel->sprites[2], 0);
    battle_text_configure_sprite_vram(&rects[5], 0x10, 0xA, &panel->sprites[3], 0);
    frame = 0;
    walk3 = g_equip_panel_sprite_cells;
    off4 = 0xEC;
    do {
        battle_gfx_init_image_loading(
            (u8*)panel + off4, g_equip_editor_numeric_geometry, &g_equip_panel_frame_rect.location, walk3);
        walk3 += 0xC;
        frame += 1;
        off4 += 0x14;
    } while (frame < 7);
    battle_gfx_init_image_loading(&panel->portrait, g_equip_editor_numeric_geometry, &g_equip_panel_frame_rect.location,
        g_equip_panel_portrait_cell);
    if (state->mode == 1) {
        panel->portrait.clut = 0x7FFD;
    } else {
        panel->portrait.clut = 0x7FBD;
    }
    panel->portrait.tpage = GetTPage(0, 1, 0x3C0, 0x100);
    battle_copy_bytes(panel + 1, panel, sizeof(battle_menu_status_panel_packet_t));
    frame = 0;
    anim_state = 0;
    prev_unit = state->unit;
    cur_unit = state->unit;
    do {
        s32 parity = frame & 1;
        panel = panel_base + parity;
        editor = editor_base + parity;
        if (state->bars[2].value >= 0x65) {
            state->bars[2].value = 0x64;
        }
        if (g_battle_post_battle_unit_changes_active == 0) {
            battle_copy_bytes(g_equip_panel_editor_mode_cell, g_equip_panel_editor_mode_cells + state->mode * 0xC, 0xC);
        } else {
            battle_copy_bytes(g_equip_panel_editor_mode_cell, g_equip_panel_editor_mode_cells, 0xC);
        }
        battle_gfx_init_image_loading(&editor->label_sprites[6], g_equip_editor_numeric_geometry,
            &g_equip_gfx_portrait_origin.location, g_equip_panel_editor_mode_cell);
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
        i = 0;
        {
            u16* clut_table;
            battle_menu_status_panel_editor_packet_t* dst;
            s32 clut_offset;
            clut_offset = highlight * 2;
            clut_table = g_equip_panel_editor_value_cluts;
            dst = editor;
            do {
                /* Launder: keeps clut_offset loop-variant so loop.c walks the table, not table + offset.
                   dst advances one SPRT per pass and the read stays integer-plus-pointer, which is
                   how the target addresses both. */
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
            clut_table = g_equip_panel_editor_label_cluts;
            dst = editor;
            do {
                /* Launder: as in the loop above. */
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
        g_equip_gfx_draw_offset_y = ((g_main_gfx_screen_polarity * 0xF0) != 0xF0) ? 0xF0 : 0;
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
                thread->shake_y.word = g_equip_panel_slide_down_y[shake];
                if (frame & 1) {
                    shake += 1;
                }
                if (shake >= 7) {
                    shake = 0;
                    thread->flags = thread->flags & 0x80;
                }
            } else if (shake >= 0xB) {
                thread->shake_y.word = g_equip_panel_slide_up_y[shake];
                if (frame & 1) {
                    shake += 1;
                }
                if (shake >= 0x11) {
                    shake = 0;
                    thread->flags = thread->flags & 0x80;
                }
            }
        }
        panel_x = thread->x;
        panel_offset_y = g_equip_gfx_draw_offset_y;
        panel->draw_offset_a.x = panel_x - 0x80;
        panel_shake_y = thread->shake_y.low;
        panel->draw_offset_a.y = panel_shake_y + panel_offset_y;
        SetDrawOffset(&panel->draw_offset_a, &panel->draw_offset_a.x);
        panel_base_offset_y = g_equip_gfx_draw_offset_y;
        panel->draw_offset_b.x = -0x80;
        panel->draw_offset_b.y = panel_base_offset_y;
        SetDrawOffset(&panel->draw_offset_b, &panel->draw_offset_b.x);
        editor_x = thread->x;
        editor_offset_y = g_equip_gfx_draw_offset_y;
        editor->draw_offset_a.x = editor_x - 0x80;
        editor_shake_y = thread->shake_y.low;
        editor->draw_offset_a.y = editor_shake_y + editor_offset_y;
        SetDrawOffset(&editor->draw_offset_a, &editor->draw_offset_a.x);
        editor_base_offset_y = g_equip_gfx_draw_offset_y;
        editor->draw_offset_b.x = -0x80;
        editor->draw_offset_b.y = editor_base_offset_y;
        SetDrawOffset(&editor->draw_offset_b, &editor->draw_offset_b.x);
        if (anim_state >= 0xA) {
            anim_state = 0;
            prev_unit = cur_unit;
            suppress = thread->flags & 0x80;
        }
        if (thread->work != 0) {
            if (anim_state == 0) {
                /* Volatile read of the named field: the request pointer is
                   fetched again rather than reused from the test above. */
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
        equip_gfx_build_portrait_transition_primitives(portrait_rect, &anim_state, &cur_unit, &prev_unit,
            (u8*)portrait_image, (POLY_FT4*)&editor->portrait[0], (s32)portrait_arg);
        {
            CVECTOR* color;
            u16* row_y;
            s32 row_offset;
            s32 prim_offset;
            battle_menu_status_panel_bar_t* bar;
            volatile battle_menu_status_panel_editor_packet_t* dst;
            color = g_equip_panel_gauge_bar_colors;
            i = 0;
            /* The bar rows read the origin y with lhu; an s16 walk changes the loads. */
            row_y = (u16*)&g_equip_gfx_portrait_origin.coordinates[1];
            row_offset = 0x18;
            dst = editor;
            prim_offset = 0x1E0;
            bar = &state->bars[0];
            /* dst walks one POLY_G4 per iteration and every store names bars[0]
               rather than bars[i]: loop.c forms no induction variables from
               volatile MEMs, so the base stays put and the displacements stay
               the positive 0x1E4-0x202 the target uses. prim_offset advances the same
               way for the SetPolyG4 argument. The volatile qualifier is also
               what keeps the stores in source order. */
            do {
                s32 limit;
                s32 py0, py1, py2, py3;
                /* Pins: unpinned, these three rotate one register up and add a copy before the x2 store. */
                register s32 x3_read __asm__("$3");
                register s32 x2_read __asm__("$2");
                register s32 x0 __asm__("$4");
                s32 width;
                SetPolyG4((u8*)editor + prim_offset);
                limit = bar->limit;
                {
                    /* Pin: loads the origin into $v0 and biases it into x0 in the delay slot. */
                    register s32 origin_x __asm__("$2");
                    origin_x = g_equip_gfx_portrait_origin.coordinates[0];
                    x0 = origin_x + 0x2F;
                }
                if (limit == 0) {
                    width = 4;
                } else {
                    width = (bar->value << 5) / limit;
                }
                if (width == 0) {
                    x0 = 0;
                }
                dst->bars[0].x0 = x0;
                py0 = *row_y;
                dst->bars[0].x1 = x0 + width;
                dst->bars[0].y0 = py0 + row_offset;
                py1 = *row_y;
                dst->bars[0].x2 = x0;
                dst->bars[0].y1 = py1 + row_offset;
                py2 = *row_y;
                dst->bars[0].x3 = x0 + width;
                x3_read = (u16)dst->bars[0].x3;
                dst->bars[0].y2 = py2 + row_offset + 3;
                py3 = *row_y;
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
                bar++;
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
            if (state->list_index < 0) {
                numeric_entries[6].format = 0xC00;
            } else {
                numeric_entries[6].format = 2;
            }
            battle_clear_menu_render_buffer(number_image, 0x700);
            {
                s32* stride;
                s32* position;
                stride = &g_menu_inner_window_width;
                *stride = 0x38;
                if (g_equip_item_numeric_thread_enabled == 0) {
                    battle_menu_draw_numeric_display_entries((s32)number_image,
                        (struct menu_number_entry*)numeric_entries, (struct menu_number_position*)(stride - 2), 6);
                } else {
                    position = stride - 2;
                    equip_text_render_decimal_entry_list((s32)number_image,
                        (battle_menu_status_panel_gauge_entry_t*)&numeric_entries[10],
                        (battle_menu_status_panel_text_position_t*)position, 1);
                    equip_text_render_decimal_entry_list((s32)number_image,
                        (battle_menu_status_panel_gauge_entry_t*)&numeric_entries[12],
                        (battle_menu_status_panel_text_position_t*)position, 1);
                    battle_menu_draw_numeric_display_entries((s32)number_image,
                        (struct menu_number_entry*)&numeric_entries[11], (struct menu_number_position*)position, 1);
                    battle_menu_draw_numeric_display_entries((s32)number_image,
                        (struct menu_number_entry*)&numeric_entries[13], (struct menu_number_position*)position, 1);
                    battle_menu_draw_numeric_display_entries((s32)number_image,
                        (struct menu_number_entry*)&numeric_entries[4], (struct menu_number_position*)position, 2);
                }
            }
            LoadImage(&rects[0], number_image);
            window_width = &g_menu_inner_window_width;
            text_position = window_width - 2;
            number_image_lower = number_image + 0x400;
            *window_width = 0x60;
            battle_menu_draw_numeric_display_entries((s32)number_image_lower,
                (struct menu_number_entry*)&numeric_entries[6], (struct menu_number_position*)text_position, 4);
            LoadImage(&rects[1], number_image_lower);
            thread->work = 0;
            battle_clear_menu_render_buffer(name_image, 0x580);
            *window_width = 0x58;
            battle_menu_set_text_origin(0, 0);
            battle_menu_display_text_entry(unit_data[0] + TEXT_ID_UNIT_NAME_BASE, name_image, text_position);
            battle_menu_set_text_origin(0, 0x10);
            battle_menu_display_text_entry(unit_data[1] + TEXT_ID_JOB_NAME_BASE, name_image, text_position);
            LoadImage(&rects[2], name_image);
            battle_clear_menu_render_buffer(small_text_image, 0xF0);
            *window_width = 0x10;
            battle_menu_set_text_origin(0, 0);
            battle_text_draw_large_number_glyphs(unit_data[2], 0x202, small_text_image, text_position);
            LoadImage(&rects[3], small_text_image);
            battle_menu_set_text_origin(0, 0);
            small_text_image_2 = small_text_image + 0x50;
            battle_text_draw_large_number_glyphs(unit_data[3], 0x202, small_text_image_2, text_position);
            LoadImage(&rects[4], small_text_image_2);
            if (g_battle_post_battle_unit_changes_active == 0) {
                battle_menu_set_text_origin(0, 0);
                small_text_image_3 = small_text_image + 0xA0;
                battle_text_draw_large_number_glyphs(unit_data[5] + 1, 0x4002, small_text_image_3, text_position);
                LoadImage(&rects[5], small_text_image_3);
            } else {
                LoadImage(&rects[5], small_text_image + 0xA0);
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
            battle_menu_submit_numeric_display_frame_primitives(&editor->numeric_frame);
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
            battle_menu_submit_numeric_display_frame_primitives(&panel->numeric_frame);
            battle_gfx_draw_or_append_gpu_primitive(&panel->draw_offset_a);
        }
        battle_thread_yield();
        done = battle_thread_get_current_parameter_3();
        /* Barrier: keeps reorg from moving `frame += 1` into the call's delay slot. */
        __asm__ __volatile__("");
        frame += 1;
    } while (done == 0);
    battle_thread_yield();
    battle_gfx_free_tpage7_vram(&rects[0]);
    battle_gfx_free_tpage7_vram(&rects[1]);
    battle_gfx_free_tpage7_vram(&rects[2]);
    battle_gfx_free_tpage7_vram(&rects[3]);
    battle_gfx_free_tpage7_vram(&rects[4]);
    battle_gfx_free_tpage7_vram(&rects[5]);
    battle_thread_exit_current();
}
