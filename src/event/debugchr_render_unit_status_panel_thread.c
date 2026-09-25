/* Profile gcc-2.6.3_O2_aspsx-2.34_divcheck: the bar-width division at
 * 0x801c021c carries the full ASPSX checked-division expansion
 * (`bnez`/`break 0x7`, then the INT_MIN/-1 test and `break 0x6`) at
 * 0x801c0220-0x801c0240. Nothing else in the range needs the exception.
 *
 * The overlay draws two alternating copies of the editor and status panels;
 * `frame & 1` picks one, and the copy stride fixes each packet's size. The
 * packet, state and thread layouts are the shared fft/battle_menu_status_panel.h records,
 * proven by this function's own accesses and the primitive strides it walks.
 * Three accesses are left in raw form because a struct
 * or pointer spelling breaks the match; each says so at its use.
 *
 * Register pins and empty asm constraints are compiler constraints that emit no
 * instructions; each is explained where it appears.
 */
#include "fft/battle_gfx.h"
#include "fft/battle_menu_status_panel.h"
#include "fft/battle_text.h"
#include "fft/debugchr.h"
#include "fft/event.h"
#include "fft/main_gfx.h"
#include "fft/main_heap.h"
#include "fft/menu.h"
#include "fft/menu_types.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/types.h"

/* The bar loop advances its destination by one POLY_G4 per iteration instead of
   indexing bars[i], so every store keeps the target's positive 0x1E4-0x202
   displacement from an unmoved base; see the note at the loop. */

struct world_menu_palette_primitives;
void battle_gfx_init_image_loading(POLY_FT4* primitive, const battle_image_location_t* base_load,
    const battle_image_location_t* base_screen, const world_gfx_image_load_parameters_t* parameters);
struct menu_number_entry;
struct menu_number_position;

/* Thread task 0x39: draws the character editor and status panels. DEBUGCHR
 * twin of attack_render_unit_status_panel_thread; this copy scales each gauge
 * bar by its value/limit and can show the zodiac sign. */
void debugchr_render_unit_status_panel_thread(void) {
    RECT rects[6];
    s32 anim_state;
    s32 cur_unit;
    s32 prev_unit;

    u8* number_pixels;                                        /* sp60 */
    battle_menu_status_panel_editor_packet_t* editor_base;    /* sp68 */
    battle_menu_status_panel_numeric_entry_t* number_entries; /* sp70 */
    battle_menu_status_panel_editor_state_t* state;           /* sp78 */
    void* portrait_request;                                   /* sp80 */
    const RECT* transition;                                   /* sp88 */
    u8* portrait_image;                                       /* sp90 */
    u8* value_pixels;                                         /* sp98 */
    u8* name_pixels;                                          /* spA0 */
    battle_menu_status_panel_packet_t* panel_base;            /* spA8 */
    s16* unit_info;                                           /* spB0 */
    battle_menu_status_panel_display_thread_t* thread;        /* spB8 */
    s32 shake;                                                /* spC0 */
    s32 highlight;                                            /* spC8 */
    s32 hide_portrait;                                        /* spD0 */

    s32 thread_id;
    s32 done;
    u8* threads;
    s32 frame;
    u8* value_geometry;
    u8* label_geometry;
    u8* panel_geometry;
    s32 i;
    s32 prim_offset;
    s32 portrait_offset;
    s32 value_sprite_offset;
    s32 label_sprite_offset;
    s32 panel_sprite_offset;
    s32 panel_x;
    s32 panel_shake_y;
    s32 editor_x;
    s32 editor_shake_y;
    s32* window_width;
    s32 panel_y_a;
    s32 panel_y_b;
    s32 editor_y_a;
    s32 editor_y_b;
    s16 portrait_cell;
    s32* text_position;
    battle_menu_status_panel_packet_t* panel;
    battle_menu_status_panel_editor_packet_t* editor;
    u8* input;
    SPRT* panel_sprites;
    u8* number_pixels_b;
    u8* value_pixels_b;
    u8* value_pixels_c;
    u8* panel_portrait;

    hide_portrait = 0;
    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_UNIT_STATUS_BANNER);
    input = (u8*)battle_script_get_controller_input_pointer(0);
    thread_id = g_battle_current_thread_id;
    threads = (u8*)g_battle_threads;
    g_debugchr_input_controller = (u32*)(input + 4);
    /* Raw index arithmetic: g_battle_threads holds one pointer per 0x400-byte
       thread slot. The equivalent `threads + (thread_id << 10)` pointer form
       moves the shift and changes the prologue. */
    thread = *(battle_menu_status_panel_display_thread_t**)((thread_id * NATIVE_THREAD_STRIDE) + (u32)threads);
    if (thread_id == 8) {
        panel = g_debugchr_panel_selected_packets;
        editor = g_debugchr_panel_selected_editor_packets;
        number_pixels = g_debugchr_panel_selected_number_image;
        value_pixels = g_debugchr_panel_selected_large_number_image;
        name_pixels = g_debugchr_panel_selected_name_image;
        number_entries = g_debugchr_panel_selected_numeric_entries;
        state = &g_debugchr_panel_selected_billboard;
        unit_info = g_debugchr_panel_selected_unit_data;
        transition = &g_debugchr_panel_selected_portrait_rect;
        panel_base = panel;
        editor_base = editor;
        portrait_image = g_debugchr_panel_selected_portrait_image;
    } else {
        panel = g_debugchr_panel_comparison_packets;
        editor = g_debugchr_panel_comparison_editor_packets;
        number_pixels = g_debugchr_panel_comparison_number_image;
        value_pixels = g_debugchr_panel_comparison_large_number_image;
        name_pixels = g_debugchr_panel_comparison_name_image;
        number_entries = g_debugchr_panel_comparison_numeric_entries;
        state = &g_debugchr_panel_comparison_billboard;
        unit_info = g_debugchr_panel_comparison_unit_data;
        transition = &g_debugchr_panel_comparison_portrait_rect;
        panel_base = panel;
        editor_base = editor;
        portrait_image = g_debugchr_panel_comparison_portrait_image;
    }
    shake = 0;

    battle_menu_init_numeric_display_frame_primitives((RECT*)g_debugchr_gfx_portrait_origin, &editor->numeric_frame);
    battle_gfx_set_draw_mode_for_texture_page(&editor->draw_mode_a, 1);
    battle_gfx_set_draw_mode_for_texture_page(&editor->draw_mode_b, 0);
    battle_menu_init_sprite_array(&editor->label_sprites[0], 7, 0x7CBC);
    battle_menu_init_sprite_array(&editor->value_sprites[0], 4, 0x7CBC);
    frame = 0;
    portrait_offset = 0x24C;
    do {
        battle_gfx_init_default_poly_ft4((u8*)editor + portrait_offset);
        frame += 1;
        portrait_offset += 0x28;
    } while (frame < 4);
    battle_text_configure_sprite_vram(&rects[0], 0x38, 0x28, &editor->value_sprites[0], 2);
    battle_text_configure_sprite_vram(&rects[1], 0x60, 0x10, &editor->value_sprites[1], 2);
    frame = 0;
    value_geometry = g_debugchr_panel_editor_value_cells;
    value_sprite_offset = 0x104;
    do {
        battle_gfx_init_image_loading((u8*)editor + value_sprite_offset, g_debugchr_editor_numeric_geometry,
            g_debugchr_gfx_portrait_origin, value_geometry);
        value_geometry += 0xC;
        frame += 1;
        value_sprite_offset += 0x14;
    } while (frame < 4);
    if (state->mode >= 4) {
        state->mode = 0;
    }
    if (g_battle_post_battle_unit_changes_active == 0) {
        battle_copy_bytes(
            g_debugchr_panel_editor_mode_cell, g_debugchr_panel_editor_mode_cells + state->mode * 0xC, 0xC);
    } else {
        battle_copy_bytes(g_debugchr_panel_editor_mode_cell, g_debugchr_panel_editor_mode_cells, 0xC);
    }
    frame = 0;
    label_geometry = g_debugchr_panel_editor_label_cells;
    label_sprite_offset = 0x154;
    do {
        battle_gfx_init_image_loading((u8*)editor + label_sprite_offset, g_debugchr_editor_numeric_geometry,
            g_debugchr_gfx_portrait_origin, label_geometry);
        label_geometry += 0xC;
        frame += 1;
        label_sprite_offset += 0x14;
    } while (frame < 7);
    battle_copy_bytes(editor + 1, editor, sizeof(battle_menu_status_panel_editor_packet_t));
    battle_gfx_set_draw_mode_for_texture_page(&panel->draw_mode_a, 0);
    battle_gfx_set_draw_mode_for_texture_page(&panel->draw_mode_b, 1);
    battle_menu_init_numeric_display_frame_primitives((RECT*)g_debugchr_panel_frame_rect, &panel->numeric_frame);
    panel_sprites = &panel->sprites[0];
    battle_menu_init_sprite_array(panel_sprites, 7, 0x7C3C);
    battle_gfx_init_default_poly_ft4(&panel->portrait);
    battle_text_configure_sprite_vram(&rects[2], 0x58, 0x20, panel_sprites, 0);
    battle_text_configure_sprite_vram(&rects[3], 0x10, 0xA, &panel->sprites[1], 0);
    battle_text_configure_sprite_vram(&rects[4], 0x10, 0xA, &panel->sprites[2], 0);
    battle_text_configure_sprite_vram(&rects[5], 0x10, 0xA, &panel->sprites[3], 0);
    frame = 0;
    panel_geometry = g_debugchr_panel_sprite_cells;
    panel_sprite_offset = 0xEC;
    do {
        battle_gfx_init_image_loading((u8*)panel + panel_sprite_offset, g_debugchr_editor_numeric_geometry,
            g_debugchr_panel_frame_rect, panel_geometry);
        panel_geometry += 0xC;
        frame += 1;
        panel_sprite_offset += 0x14;
    } while (frame < 7);
    battle_gfx_init_image_loading(&panel->portrait, g_debugchr_editor_numeric_geometry, g_debugchr_panel_frame_rect,
        g_debugchr_panel_portrait_cell);
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
            battle_copy_bytes(
                g_debugchr_panel_editor_mode_cell, g_debugchr_panel_editor_mode_cells + state->mode * 0xC, 0xC);
        } else {
            battle_copy_bytes(g_debugchr_panel_editor_mode_cell, g_debugchr_panel_editor_mode_cells, 0xC);
        }
        battle_gfx_init_image_loading(&editor->label_sprites[6], g_debugchr_editor_numeric_geometry,
            g_debugchr_gfx_portrait_origin, g_debugchr_panel_editor_mode_cell);
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
            u16* cluts;
            battle_menu_status_panel_editor_packet_t* packet;
            s32 clut_column;
            clut_column = highlight * 2;
            cluts = g_debugchr_panel_editor_value_cluts;
            packet = editor;
            do {
                /* A loop-local set of the byte offset stops loop.c from making
                   it an induction variable, so the table read stays a register
                   add rather than a second advancing pointer. The destination
                   advances one SPRT per iteration instead of indexing
                   value_sprites[i], which is how the target biases it.
                   The table read keeps integer-plus-pointer form: the
                   equivalent `(u8*)cluts + clut_column` changes the emitted address
                   arithmetic. */
                __asm__("" : "=r"(clut_column) : "0"(clut_column));
                packet->value_sprites[0].clut = *(u16*)(clut_column + (u32)cluts);
                cluts += 2;
                i += 1;
                packet = (battle_menu_status_panel_editor_packet_t*)((u8*)packet + sizeof(SPRT));
            } while (i < 4);
        }
        i = 0;
        {
            u16* cluts;
            battle_menu_status_panel_editor_packet_t* packet;
            s32 clut_column;
            clut_column = highlight * 2;
            cluts = g_debugchr_panel_editor_label_cluts;
            packet = editor;
            do {
                /* Same invariant-motion block as the loop above. */
                __asm__("" : "=r"(clut_column) : "0"(clut_column));
                packet->label_sprites[0].clut = *(u16*)(clut_column + (u32)cluts);
                cluts += 2;
                i += 1;
                packet = (battle_menu_status_panel_editor_packet_t*)((u8*)packet + sizeof(SPRT));
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
        g_debugchr_gfx_draw_offset_y = ((g_main_gfx_screen_polarity * 0xF0) != 0xF0) ? 0xF0 : 0;
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
                thread->shake_y.word = g_debugchr_panel_slide_down_y[shake];
                if (frame & 1) {
                    shake += 1;
                }
                if (shake >= 7) {
                    shake = 0;
                    thread->flags = thread->flags & 0x80;
                }
            } else if (shake >= 0xB) {
                thread->shake_y.word = g_debugchr_panel_slide_up_y[shake];
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
        panel_y_a = g_debugchr_gfx_draw_offset_y;
        panel->draw_offset_a.x = panel_x - 0x80;
        panel_shake_y = thread->shake_y.low;
        panel->draw_offset_a.y = panel_shake_y + panel_y_a;
        SetDrawOffset(&panel->draw_offset_a, &panel->draw_offset_a.x);
        panel_y_b = g_debugchr_gfx_draw_offset_y;
        panel->draw_offset_b.x = -0x80;
        panel->draw_offset_b.y = panel_y_b;
        SetDrawOffset(&panel->draw_offset_b, &panel->draw_offset_b.x);
        editor_x = thread->x;
        editor_y_a = g_debugchr_gfx_draw_offset_y;
        editor->draw_offset_a.x = editor_x - 0x80;
        editor_shake_y = thread->shake_y.low;
        editor->draw_offset_a.y = editor_shake_y + editor_y_a;
        SetDrawOffset(&editor->draw_offset_a, &editor->draw_offset_a.x);
        editor_y_b = g_debugchr_gfx_draw_offset_y;
        editor->draw_offset_b.x = -0x80;
        editor->draw_offset_b.y = editor_y_b;
        SetDrawOffset(&editor->draw_offset_b, &editor->draw_offset_b.x);
        if (anim_state >= 0xA) {
            anim_state = 0;
            prev_unit = cur_unit;
            hide_portrait = thread->flags & 0x80;
        }
        if (thread->work != 0) {
            if (anim_state == 0) {
                /* Volatile read of the named field: the request pointer is
                   fetched again rather than reused from the test above. */
                portrait_request = *(void* volatile*)&thread->work;
                prev_unit = cur_unit;
                cur_unit = state->unit;
            } else {
                portrait_request = *(void* volatile*)&thread->work;
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
        debugchr_gfx_build_portrait_transition_primitives(transition, &anim_state, &cur_unit, &prev_unit,
            (u8*)portrait_image, (POLY_FT4*)&editor->portrait[0], (s32)portrait_request);
        {
            CVECTOR* color;
            u16* bar_origin_y;
            s32 bar_y;
            s32 bar_offset;
            battle_menu_status_panel_bar_t* bar;
            volatile battle_menu_status_panel_editor_packet_t* packet;
            color = g_debugchr_panel_gauge_bar_colors;
            i = 0;
            /* The bar rows read the origin y with lhu; an s16 walk changes the loads. */
            bar_origin_y = (u16*)&g_debugchr_gfx_portrait_origin[1];
            bar_y = 0x18;
            packet = editor;
            bar_offset = 0x1E0;
            bar = &state->bars[0];
            /* packet walks one POLY_G4 per iteration and every store names bars[0]
               rather than bars[i]: loop.c forms no induction variables from
               volatile MEMs, so the base stays put and the displacements stay
               the positive 0x1E4-0x202 the target uses. bar_offset advances the same
               way for the SetPolyG4 argument. The volatile qualifier is also
               what keeps the stores in source order. */
            do {
                s32 limit;
                s32 py0, py1, py2, py3;
                /* Reload spills this loop's primitive offset and row pointer
                   around SetPolyG4 and reuses $4-$6 for them. Left to the
                   allocator the three temporaries below rotate one register up,
                   which also costs a redundant copy before the x2 store.
                   Pinning them fixes the rotation; none is live across a call,
                   and $2/$3/$4 stay ordinary scratch elsewhere. */
                register s32 x3_read __asm__("$3");
                register s32 x2_read __asm__("$2");
                register s32 x0 __asm__("$4");
                s32 width;
                SetPolyG4((u8*)editor + bar_offset);
                limit = bar->limit;
                {
                    /* With x0 pinned the load would target $4 directly; the
                       target loads the portrait origin into $2 first and only biases it
                       in the branch delay slot. */
                    register s32 origin_x __asm__("$2");
                    origin_x = g_debugchr_gfx_portrait_origin[0];
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
                packet->bars[0].x0 = x0;
                py0 = *bar_origin_y;
                packet->bars[0].x1 = x0 + width;
                packet->bars[0].y0 = py0 + bar_y;
                py1 = *bar_origin_y;
                packet->bars[0].x2 = x0;
                packet->bars[0].y1 = py1 + bar_y;
                py2 = *bar_origin_y;
                packet->bars[0].x3 = x0 + width;
                x3_read = (u16)packet->bars[0].x3;
                packet->bars[0].y2 = py2 + bar_y + 3;
                py3 = *bar_origin_y;
                x2_read = (u16)packet->bars[0].x2;
                packet->bars[0].x3 = x3_read - 3;
                packet->bars[0].x2 = x2_read - 3;
                packet->bars[0].y3 = py3 + bar_y + 3;
                if (highlight != 0) {
                    packet->bars[0].r0 = color->r >> 1;
                    packet->bars[0].g0 = color->g >> 1;
                    packet->bars[0].b0 = color->b;
                    color++;
                    packet->bars[0].r1 = color->r >> 1;
                    packet->bars[0].g1 = color->g >> 1;
                    packet->bars[0].b1 = color->b;
                    color++;
                    packet->bars[0].r2 = color->r >> 1;
                    packet->bars[0].g2 = color->g >> 1;
                    packet->bars[0].b2 = color->b;
                    color++;
                    packet->bars[0].r3 = color->r >> 1;
                    packet->bars[0].g3 = color->g >> 1;
                    packet->bars[0].b3 = color->b;
                    color++;
                } else {
                    packet->bars[0].r0 = color->r;
                    packet->bars[0].g0 = color->g;
                    packet->bars[0].b0 = color->b;
                    color++;
                    packet->bars[0].r1 = color->r;
                    packet->bars[0].g1 = color->g;
                    packet->bars[0].b1 = color->b;
                    color++;
                    packet->bars[0].r2 = color->r;
                    packet->bars[0].g2 = color->g;
                    packet->bars[0].b2 = color->b;
                    color++;
                    packet->bars[0].r3 = color->r;
                    packet->bars[0].g3 = color->g;
                    packet->bars[0].b3 = color->b;
                    color++;
                }
                bar++;
                bar_y += 0xB;
                packet = (volatile battle_menu_status_panel_editor_packet_t*)((u8*)packet + sizeof(POLY_G4));
                i += 1;
                bar_offset += 0x24;
            } while (i < 3);
        }
        panel->sprites[6].u0 = (s16)(unit_info[4] % 7) * 0x18;
        portrait_cell = unit_info[4];
        panel->sprites[6].w = 0x18;
        panel->sprites[6].h = 0x14;
        panel->sprites[6].v0 = (s16)(portrait_cell / 7) * 0x14 + 0x2A;
        if ((frame == 0) || (thread->work != 0)) {
            if (state->list_index < 0) {
                number_entries[6].format = 0xC00;
            } else {
                number_entries[6].format = 2;
            }
            battle_clear_menu_render_buffer(number_pixels, 0x700);
            window_width = &g_menu_inner_window_width;
            text_position = window_width - 2;
            *window_width = 0x38;
            battle_menu_draw_numeric_display_entries((s32)number_pixels, (struct menu_number_entry*)number_entries,
                (struct menu_number_position*)text_position, 6);
            LoadImage(&rects[0], number_pixels);
            number_pixels_b = number_pixels + 0x400;
            *window_width = 0x60;
            battle_menu_draw_numeric_display_entries((s32)number_pixels_b,
                (struct menu_number_entry*)&number_entries[6], (struct menu_number_position*)text_position, 4);
            LoadImage(&rects[1], number_pixels_b);
            thread->work = 0;
            battle_clear_menu_render_buffer(name_pixels, 0x580);
            *window_width = 0x58;
            battle_menu_set_text_origin(0, 0);
            battle_menu_display_text_entry(unit_info[0] + TEXT_ID_UNIT_NAME_BASE, name_pixels, text_position);
            battle_menu_set_text_origin(0, 0x10);
            battle_menu_display_text_entry(unit_info[1] + TEXT_ID_JOB_NAME_BASE, name_pixels, text_position);
            LoadImage(&rects[2], name_pixels);
            battle_clear_menu_render_buffer(value_pixels, 0xF0);
            *window_width = 0x10;
            battle_menu_set_text_origin(0, 0);
            battle_text_draw_large_number_glyphs(unit_info[2], 0x202, value_pixels, text_position);
            LoadImage(&rects[3], value_pixels);
            battle_menu_set_text_origin(0, 0);
            value_pixels_b = value_pixels + 0x50;
            battle_text_draw_large_number_glyphs(unit_info[3], 0x202, value_pixels_b, text_position);
            LoadImage(&rects[4], value_pixels_b);
            if (g_battle_post_battle_unit_changes_active == 0) {
                if (g_debugchr_editor_active != 0) {
                    battle_menu_set_text_origin(0, 0);
                    value_pixels_c = value_pixels + 0xA0;
                    battle_text_draw_large_number_glyphs(unit_info[5] + 1, 0x4002, value_pixels_c, text_position);
                    LoadImage(&rects[5], value_pixels_c);
                } else {
                    s32 zodiac;
                    battle_menu_set_text_origin(0, 0);
                    zodiac = unit_info[5];
                    if (zodiac >= 0x10) {
                        battle_text_draw_large_number_glyphs(zodiac - 0xF, 0x4002, value_pixels + 0xA0, text_position);
                    }
                    LoadImage(&rects[5], value_pixels + 0xA0);
                }
            } else {
                LoadImage(&rects[5], value_pixels + 0xA0);
            }
        }
        if (!(thread->flags & 0x80)) {
            battle_gfx_draw_or_append_gpu_primitive(&editor->draw_offset_b);
            battle_gfx_draw_or_append_gpu_primitive(&editor->value_sprites[0]);
            battle_gfx_draw_or_append_gpu_primitive(&editor->value_sprites[1]);
            battle_gfx_draw_or_append_gpu_primitive(&editor->draw_mode_a);
            i = 0;
            prim_offset = 0x1E0;
            do {
                battle_gfx_draw_or_append_gpu_primitive((u8*)editor + prim_offset);
                i += 1;
                prim_offset += 0x24;
            } while (i < 3);
            battle_gfx_draw_or_append_gpu_primitive(&editor->value_sprites[2]);
            battle_gfx_draw_or_append_gpu_primitive(&editor->value_sprites[3]);
            i = 0;
            prim_offset = 0x154;
            do {
                battle_gfx_draw_or_append_gpu_primitive((u8*)editor + prim_offset);
                i += 1;
                prim_offset += 0x14;
            } while (i < 7);
            battle_gfx_draw_or_append_gpu_primitive(&editor->draw_mode_b);
            if (hide_portrait == 0) {
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
            panel_portrait = (u8*)&panel->portrait;
            SetSemiTrans(panel_portrait, 1);
            battle_gfx_draw_or_append_gpu_primitive(panel_portrait);
            i = 3;
            prim_offset = 0x128;
            do {
                battle_gfx_draw_or_append_gpu_primitive((u8*)panel + prim_offset);
                i -= 1;
                prim_offset -= 0x14;
            } while (i >= 0);
            battle_gfx_draw_or_append_gpu_primitive(&panel->draw_mode_b);
            i = 4;
            prim_offset = 0x13C;
            do {
                battle_gfx_draw_or_append_gpu_primitive((u8*)panel + prim_offset);
                i += 1;
                prim_offset += 0x14;
            } while (i < 7);
            battle_gfx_draw_or_append_gpu_primitive(&panel->draw_mode_a);
            battle_menu_submit_numeric_display_frame_primitives(&panel->numeric_frame);
            battle_gfx_draw_or_append_gpu_primitive(&panel->draw_offset_a);
        }
        battle_thread_yield();
        done = battle_thread_get_current_parameter_3();
        /* reorg.c fills call delay slots before branch delay slots and will
           take the frame increment for the call above. Blocking that leaves it
           adjacent to the loop-back branch, which is where the target keeps it;
           the loop then re-enters at the parity computation. */
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
