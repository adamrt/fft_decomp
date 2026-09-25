#include "fft/main_gfx.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/types.h"

/* Provisional 0x30C-byte unit-summary primitive packet, doubled back to back
 * at 0x801C0584 (thread 8) and 0x801C0F4C (other). Same layout as the EVENT
 * DEBUGCHR twin's debugchr_editor_packet_t. */
typedef struct world_formation_summary_packet {
    world_menu_palette_primitives_t frame; /* 0x000: world_menu_build_line_box */
    u8 unknown_frame_tail[0xEC - sizeof(world_menu_palette_primitives_t)];
    DR_MODE draw_mode_a;                                  /* 0x0EC */
    DR_MODE draw_mode_b;                                  /* 0x0F8 */
    SPRT value_sprites[4];                                /* 0x104 */
    SPRT label_sprites[7];                                /* 0x154; [6] at 0x1CC is the mode row */
    POLY_G4 bars[3];                                      /* 0x1E0 */
    POLY_FT4 portrait[4];                                 /* 0x24C */
    battle_menu_status_panel_draw_offset_t draw_offset_a; /* 0x2EC */
    battle_menu_status_panel_draw_offset_t draw_offset_b; /* 0x2FC */
} world_formation_summary_packet_t;

/* Provisional 0x1D8-byte status-panel packet, doubled at 0x801C0B9C (thread 8)
 * and 0x801C1564 (other). DEBUGCHR twin: debugchr_panel_packet_t. */
typedef struct world_formation_status_packet {
    world_menu_palette_primitives_t frame; /* 0x000 */
    u8 unknown_frame_tail[0xEC - sizeof(world_menu_palette_primitives_t)];
    SPRT sprites[7];                                      /* 0x0EC; [6] is the zodiac/scroll cursor */
    DR_MODE draw_mode_a;                                  /* 0x178 */
    DR_MODE draw_mode_b;                                  /* 0x184 */
    battle_menu_status_panel_draw_offset_t draw_offset_a; /* 0x190 */
    battle_menu_status_panel_draw_offset_t draw_offset_b; /* 0x1A0 */
    POLY_FT4 portrait;                                    /* 0x1B0 */
} world_formation_status_packet_t;

typedef struct world_formation_bar {
    s16 value;
    s16 bonus;
    s16 limit;
} world_formation_bar_t;

/* Provisional 0x24-byte displayed-unit record at 0x8018AA98 / 0x8018AABC. */
typedef struct world_formation_unit_state {
    s16 unknown_00;
    s16 mode;       /* 0x02 */
    s16 list_index; /* 0x04: AT-list position; dashes (format 0xc00) when negative */
    s16 unknown_06;
    s16 unknown_08;
    s16 unit;                      /* 0x0A */
    world_formation_bar_t bars[3]; /* 0x0C */
    s16 unknown_1e[3];
} world_formation_unit_state_t;

/* Display record the thread's first parameter points at. */
typedef struct world_formation_display {
    u16 x; /* 0x00 */
    u16 unknown_02;
    union {
        s32 word; /* 0x04 */
        u16 low;
    } shake_y;
    s32 flags;     /* 0x08: 0x20/0x40 start the shake tables, 0x80 hides */
    void* work;    /* 0x0C: redraw request */
    s32 highlight; /* 0x10: nonzero forces the highlight CLUTs (g_world_status_display_thread_params.style,
                      g_world_comparison_display_thread_params.style) */
} world_formation_display_t;

#define SUMMARY(p)  ((world_formation_summary_packet_t*)(p))
#define STATUS(p)   ((world_formation_status_packet_t*)(p))
#define VSUMMARY(p) ((volatile world_formation_summary_packet_t*)(p))

void world_menu_unit_status_banner_thread(void) {
    RECT rects[6];
    s32 anim_state;
    s32 cur_unit;
    s32 prev_unit;

    u8* render_a;                           /* sp60 */
    u8* summary_base;                       /* sp68 */
    world_menu_number_entry_t* entries;     /* sp70 */
    world_formation_unit_state_t* state;    /* sp78 */
    void* portrait_arg;                     /* sp80 */
    u8* transition;                         /* sp88 */
    u8* portrait_image;                     /* sp90 */
    u8* render_b;                           /* sp98 */
    u8* render_c;                           /* spA0 */
    u8* status_base;                        /* spA8 */
    world_unit_status_identity_t* identity; /* spB0 */
    world_formation_display_t* display;     /* spB8 */
    s32 frame;                              /* spC0 */
    s32 shake;                              /* spC8 */
    s32 highlight;                          /* spD0 */
    s32 suppress;                           /* spD8 */

    s32 thread_id;
    u8* threads;
    const world_gfx_image_load_parameters_t* walk;
    s32 i;
    s32 offset;
    s32 draw_x;
    s32 draw_y;
    s16 cell;
    s32* text_stride;
    s32* origin;
    u8* status;
    u8* summary;
    u8* cur;
    u8* render_a_upper;
    u8* render_b_second;
    u8* render_b_third;
    u8* portrait_prim;

    suppress = 0;
    world_thread_set_current_task_id(NATIVE_THREAD_TASK_UNIT_STATUS_BANNER);
    cur = (u8*)world_input_get_menu_controller(0);
    thread_id = g_world_thread_current_id;
    threads = (u8*)g_world_threads;
    g_world_input_frame_controller_input = (u32*)(cur + 4);
    display = *(world_formation_display_t**)((thread_id * NATIVE_THREAD_STRIDE) + (u32)threads);
    if (thread_id == 8) {
        status = g_world_selected_unit_status_packets;
        summary = g_world_selected_unit_summary_packets;
        render_a = g_world_selected_unit_stat_text_image;
        render_b = g_world_selected_unit_number_text_image;
        render_c = g_world_selected_unit_name_text_image;
        entries = g_world_selected_unit_number_entries;
        state = (world_formation_unit_state_t*)&g_world_selected_unit_stat_summary;
        identity = &g_world_selected_unit_identity;
        transition = g_world_selected_unit_portrait_rect;
        status_base = status;
        summary_base = summary;
        portrait_image = g_world_selected_unit_portrait_image;
    } else {
        status = g_world_comparison_unit_status_packets;
        summary = g_world_comparison_unit_summary_packets;
        render_a = g_world_comparison_unit_stat_text_image;
        render_b = g_world_comparison_unit_number_text_image;
        render_c = g_world_comparison_unit_name_text_image;
        entries = g_world_comparison_unit_number_entries;
        state = (world_formation_unit_state_t*)g_world_comparison_unit_stat_summary;
        identity = &g_world_comparison_unit_identity;
        transition = g_world_comparison_unit_portrait_rect;
        status_base = status;
        summary_base = summary;
        portrait_image = g_world_comparison_unit_portrait_image;
    }
    shake = 0;

    world_menu_build_line_box((RECT*)g_world_gfx_portrait_origin, &SUMMARY(summary)->frame);
    world_gfx_set_image_draw_mode(&SUMMARY(summary)->draw_mode_a, 1);
    world_gfx_set_image_draw_mode(&SUMMARY(summary)->draw_mode_b, 0);
    world_menu_init_sprite_array(&SUMMARY(summary)->label_sprites[0], 7, 0x7CBC);
    world_menu_init_sprite_array(&SUMMARY(summary)->value_sprites[0], 4, 0x7CBC);
    frame = 0;
    offset = 0x24C;
    do {
        world_menu_init_quad((POLY_FT4*)(summary + offset));
        frame += 1;
        offset += 0x28;
    } while (frame < 4);
    world_menu_init_icon_slot(&rects[0], 0x38, 0x28, (world_texture_prim_t*)&SUMMARY(summary)->value_sprites[0], 2);
    world_menu_init_icon_slot(&rects[1], 0x60, 0x10, (world_texture_prim_t*)&SUMMARY(summary)->value_sprites[1], 2);
    frame = 0;
    walk = g_world_unit_summary_value_sprite_params;
    offset = 0x104;
    do {
        world_gfx_init_image_loading((POLY_FT4*)(summary + offset),
            (const world_image_location_t*)g_world_editor_numeric_geometry,
            (const world_image_location_t*)g_world_gfx_portrait_origin, walk);
        walk++;
        frame += 1;
        offset += 0x14;
    } while (frame < 4);
    if (state->mode >= 4) {
        state->mode = 0;
    }
    if (g_world_unit_join_screen_active == 0) {
        world_script_copy_bytes(&g_world_unit_summary_label_sprite_params[6],
            g_world_unit_summary_mode_row_params + state->mode * 0xC, 0xC);
    } else {
        world_script_copy_bytes(
            &g_world_unit_summary_label_sprite_params[6], g_world_unit_summary_mode_row_params, 0xC);
    }
    frame = 0;
    walk = g_world_unit_summary_label_sprite_params;
    offset = 0x154;
    do {
        world_gfx_init_image_loading((POLY_FT4*)(summary + offset),
            (const world_image_location_t*)g_world_editor_numeric_geometry,
            (const world_image_location_t*)g_world_gfx_portrait_origin, walk);
        walk++;
        frame += 1;
        offset += 0x14;
    } while (frame < 7);
    world_script_copy_bytes(
        summary + sizeof(world_formation_summary_packet_t), summary, sizeof(world_formation_summary_packet_t));
    world_gfx_set_image_draw_mode(&STATUS(status)->draw_mode_a, 0);
    world_gfx_set_image_draw_mode(&STATUS(status)->draw_mode_b, 1);
    world_menu_build_line_box((RECT*)g_world_unit_status_panel_origin, &STATUS(status)->frame);
    cur = (u8*)&STATUS(status)->sprites[0];
    world_menu_init_sprite_array((SPRT*)cur, 7, 0x7C3C);
    world_menu_init_quad(&STATUS(status)->portrait);
    world_menu_init_icon_slot(&rects[2], 0x58, 0x20, (world_texture_prim_t*)cur, 0);
    world_menu_init_icon_slot(&rects[3], 0x10, 0xA, (world_texture_prim_t*)&STATUS(status)->sprites[1], 0);
    world_menu_init_icon_slot(&rects[4], 0x10, 0xA, (world_texture_prim_t*)&STATUS(status)->sprites[2], 0);
    world_menu_init_icon_slot(&rects[5], 0x10, 0xA, (world_texture_prim_t*)&STATUS(status)->sprites[3], 0);
    frame = 0;
    walk = g_world_unit_status_sprite_params;
    offset = 0xEC;
    do {
        world_gfx_init_image_loading((POLY_FT4*)(status + offset),
            (const world_image_location_t*)g_world_editor_numeric_geometry,
            (const world_image_location_t*)g_world_unit_status_panel_origin, walk);
        walk++;
        frame += 1;
        offset += 0x14;
    } while (frame < 7);
    world_gfx_init_image_loading(&STATUS(status)->portrait,
        (const world_image_location_t*)g_world_editor_numeric_geometry,
        (const world_image_location_t*)g_world_unit_status_panel_origin,
        (const world_gfx_image_load_parameters_t*)g_world_unit_status_portrait_params);
    if (state->mode == 1) {
        STATUS(status)->portrait.clut = 0x7FFD;
    } else {
        STATUS(status)->portrait.clut = 0x7FBD;
    }
    STATUS(status)->portrait.tpage = GetTPage(0, 1, 0x3C0, 0x100);
    world_script_copy_bytes(
        status + sizeof(world_formation_status_packet_t), status, sizeof(world_formation_status_packet_t));
    frame = 0;
    anim_state = 0;
    prev_unit = state->unit;
    cur_unit = state->unit;
    for (;;) {
        s32 parity = frame & 1;
        status = status_base + parity * sizeof(world_formation_status_packet_t);
        summary = summary_base + parity * sizeof(world_formation_summary_packet_t);
        if (state->bars[2].value >= 0x65) {
            state->bars[2].value = 0x64;
        }
        if (g_world_unit_join_screen_active == 0) {
            world_script_copy_bytes(&g_world_unit_summary_label_sprite_params[6],
                g_world_unit_summary_mode_row_params + state->mode * 0xC, 0xC);
        } else {
            world_script_copy_bytes(
                &g_world_unit_summary_label_sprite_params[6], g_world_unit_summary_mode_row_params, 0xC);
        }
        world_gfx_init_image_loading((POLY_FT4*)&SUMMARY(summary)->label_sprites[6],
            (const world_image_location_t*)g_world_editor_numeric_geometry,
            (const world_image_location_t*)g_world_gfx_portrait_origin, &g_world_unit_summary_label_sprite_params[6]);
        if (state->mode == 1) {
            STATUS(status)->portrait.clut = 0x7FFD;
        } else {
            STATUS(status)->portrait.clut = 0x7FBD;
        }
        highlight = 0;
        if (world_thread_find_running_by_task(NATIVE_THREAD_TASK_STATUS_PANEL) != 0) {
            highlight = g_world_thread_task_active != 0;
        }
        if (display->highlight != 0) {
            highlight = 1;
        }
        i = 0;
        {
            u16* src;
            u8* dst;
            s32 clut_offset;
            clut_offset = highlight * 2;
            src = g_world_unit_summary_value_cluts;
            dst = summary;
            do {
                /* Keeps clut_offset + src recomputed each pass; otherwise loop
                 * strength reduction folds them into one walking pointer. */
                __asm__("" : "=r"(clut_offset) : "0"(clut_offset));
                SUMMARY(dst)->value_sprites[0].clut = *(u16*)(clut_offset + (u32)src);
                src += 2;
                i += 1;
                dst += sizeof(SPRT);
            } while (i < 4);
        }
        i = 0;
        {
            u16* src;
            u8* dst;
            s32 clut_offset;
            clut_offset = highlight * 2;
            src = g_world_unit_summary_label_cluts;
            dst = summary;
            do {
                /* Keeps clut_offset + src recomputed each pass; otherwise loop
                 * strength reduction folds them into one walking pointer. */
                __asm__("" : "=r"(clut_offset) : "0"(clut_offset));
                SUMMARY(dst)->label_sprites[0].clut = *(u16*)(clut_offset + (u32)src);
                src += 2;
                i += 1;
                dst += sizeof(SPRT);
            } while (i < 7);
        }
        i = 0;
        offset = 0;
        do {
            if (highlight != 0) {
                STATUS(status + offset)->sprites[0].clut = 0x7D3C;
            } else {
                STATUS(status + offset)->sprites[0].clut = 0x7C3C;
            }
            i += 1;
            offset += 0x14;
        } while (i < 7);
        if (highlight != 0) {
            world_menu_init_primitive_colors_palette_bank_1(&SUMMARY(summary)->frame);
            world_menu_init_primitive_colors_palette_bank_1(&STATUS(status)->frame);
            if ((u32)(anim_state - 4) < 6U) {
                SUMMARY(summary)->portrait[0].r0 = 0x20;
                SUMMARY(summary)->portrait[0].g0 = 0x28;
                SUMMARY(summary)->portrait[0].b0 = 0x38;
            } else {
                SUMMARY(summary)->portrait[0].r0 = 0x40;
                SUMMARY(summary)->portrait[0].g0 = 0x50;
                SUMMARY(summary)->portrait[0].b0 = 0x70;
            }
            SUMMARY(summary)->portrait[1].r0 = 0x20;
            SUMMARY(summary)->portrait[1].g0 = 0x28;
            SUMMARY(summary)->portrait[1].b0 = 0x38;
            SUMMARY(summary)->portrait[2].r0 = 0x20;
            SUMMARY(summary)->portrait[2].g0 = 0x28;
            SUMMARY(summary)->portrait[2].b0 = 0x38;
            SUMMARY(summary)->portrait[3].r0 = 0x20;
            SUMMARY(summary)->portrait[3].g0 = 0x28;
            SUMMARY(summary)->portrait[3].b0 = 0x38;
        } else {
            s32 shade;
            world_menu_init_primitive_colors_palette_bank_0(&SUMMARY(summary)->frame);
            world_menu_init_primitive_colors_palette_bank_0(&STATUS(status)->frame);
            if ((u32)(anim_state - 4) >= 6U) {
                SUMMARY(summary)->portrait[0].r0 = 0x80;
                SUMMARY(summary)->portrait[0].g0 = 0x80;
                SUMMARY(summary)->portrait[0].b0 = 0x80;
            } else {
                SUMMARY(summary)->portrait[0].r0 = 0x40;
                SUMMARY(summary)->portrait[0].g0 = 0x40;
                SUMMARY(summary)->portrait[0].b0 = 0x40;
            }
            shade = 0x40;
            SUMMARY(summary)->portrait[1].r0 = shade;
            SUMMARY(summary)->portrait[1].g0 = shade;
            SUMMARY(summary)->portrait[1].b0 = shade;
            SUMMARY(summary)->portrait[2].r0 = shade;
            SUMMARY(summary)->portrait[2].g0 = shade;
            SUMMARY(summary)->portrait[2].b0 = shade;
            SUMMARY(summary)->portrait[3].r0 = shade;
            SUMMARY(summary)->portrait[3].g0 = shade;
            SUMMARY(summary)->portrait[3].b0 = shade;
        }
        if (highlight != 0) {
            STATUS(status)->portrait.r0 = 0x40;
            STATUS(status)->portrait.g0 = 0x40;
            STATUS(status)->portrait.b0 = 0x60;
        } else {
            STATUS(status)->portrait.r0 = 0x80;
            STATUS(status)->portrait.g0 = 0x80;
            STATUS(status)->portrait.b0 = 0x80;
        }
        g_world_gfx_draw_area_y = ((u16)g_world_frame_arg != 0xF0) ? 0xF0 : 0;
        if ((display->flags & 0x20) && (shake == 0)) {
            shake = 1;
        }
        if (display->flags & 0x40) {
            if (shake == 0) {
                shake = 0xB;
            }
        }
        if (shake != 0) {
            if (shake < 0xA) {
                display->shake_y.word = g_world_panel_slide_down_y[shake];
                if (frame & 1) {
                    shake += 1;
                }
                if (shake >= 7) {
                    shake = 0;
                    display->flags = display->flags & 0x80;
                }
            } else if (shake >= 0xB) {
                display->shake_y.word = g_world_panel_slide_up_y[shake];
                if (frame & 1) {
                    shake += 1;
                }
                if (shake >= 0x11) {
                    shake = 0;
                    display->flags = display->flags & 0x80;
                }
            }
        }
        draw_x = display->x;
        draw_y = (u16)g_world_gfx_draw_area_y;
        STATUS(status)->draw_offset_a.x = draw_x - 0x80;
        STATUS(status)->draw_offset_a.y = display->shake_y.low + draw_y;
        SetDrawOffset(&STATUS(status)->draw_offset_a, &STATUS(status)->draw_offset_a.x);
        STATUS(status)->draw_offset_b.x = -0x80;
        STATUS(status)->draw_offset_b.y = g_world_gfx_draw_area_y;
        SetDrawOffset(&STATUS(status)->draw_offset_b, &STATUS(status)->draw_offset_b.x);
        draw_x = display->x;
        draw_y = (u16)g_world_gfx_draw_area_y;
        SUMMARY(summary)->draw_offset_a.x = draw_x - 0x80;
        SUMMARY(summary)->draw_offset_a.y = display->shake_y.low + draw_y;
        SetDrawOffset(&SUMMARY(summary)->draw_offset_a, &SUMMARY(summary)->draw_offset_a.x);
        SUMMARY(summary)->draw_offset_b.x = -0x80;
        SUMMARY(summary)->draw_offset_b.y = g_world_gfx_draw_area_y;
        SetDrawOffset(&SUMMARY(summary)->draw_offset_b, &SUMMARY(summary)->draw_offset_b.x);
        if (anim_state >= 0xA) {
            anim_state = 0;
            prev_unit = cur_unit;
            suppress = display->flags & 0x80;
        }
        if (display->work != 0) {
            if (anim_state == 0) {
                portrait_arg = *(void* volatile*)&display->work;
                prev_unit = cur_unit;
                cur_unit = state->unit;
            } else {
                portrait_arg = *(void* volatile*)&display->work;
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
        world_formation_build_portrait_transition_primitives((const RECT*)transition, &anim_state, &cur_unit,
            &prev_unit, portrait_image, &SUMMARY(summary)->portrait[0], (s32)portrait_arg);
        {
            CVECTOR* color;
            u16* bar_origin;
            s32 row_offset;
            s32 bar_offset;
            u8* dst;
            color = g_world_unit_panel_bar_colors;
            i = 0;
            bar_origin = (u16*)g_world_gfx_portrait_origin;
            row_offset = 0x18;
            dst = summary;
            bar_offset = 0x1E0;
            do {
                s32 x;
                s32 x0;
                s32 x1;
                s32 bar_y0, bar_y1, bar_y2, bar_y3;
                /* Adjusting both reads in place before either store keeps
                   the target's register rotation. */
                s32 x3_read;
                s32 x2_read;
                SetPolyG4(summary + bar_offset);
                x = (s16)bar_origin[0];
                x0 = x + 0x2F;
                x1 = x + 0x4F;
                VSUMMARY(dst)->bars[0].x0 = x0;
                bar_y0 = bar_origin[1];
                VSUMMARY(dst)->bars[0].x1 = x1;
                VSUMMARY(dst)->bars[0].y0 = bar_y0 + row_offset;
                bar_y1 = bar_origin[1];
                VSUMMARY(dst)->bars[0].x2 = x0;
                VSUMMARY(dst)->bars[0].y1 = bar_y1 + row_offset;
                bar_y2 = bar_origin[1];
                VSUMMARY(dst)->bars[0].x3 = x1;
                x3_read = (u16)VSUMMARY(dst)->bars[0].x3;
                VSUMMARY(dst)->bars[0].y2 = bar_y2 + row_offset + 3;
                bar_y3 = bar_origin[1];
                x2_read = (u16)VSUMMARY(dst)->bars[0].x2;
                x3_read -= 3;
                x2_read -= 3;
                VSUMMARY(dst)->bars[0].x3 = x3_read;
                VSUMMARY(dst)->bars[0].x2 = x2_read;
                VSUMMARY(dst)->bars[0].y3 = bar_y3 + row_offset + 3;
                if (highlight != 0) {
                    VSUMMARY(dst)->bars[0].r0 = color->r >> 1;
                    VSUMMARY(dst)->bars[0].g0 = color->g >> 1;
                    VSUMMARY(dst)->bars[0].b0 = color->b;
                    color++;
                    VSUMMARY(dst)->bars[0].r1 = color->r >> 1;
                    VSUMMARY(dst)->bars[0].g1 = color->g >> 1;
                    VSUMMARY(dst)->bars[0].b1 = color->b;
                    color++;
                    VSUMMARY(dst)->bars[0].r2 = color->r >> 1;
                    VSUMMARY(dst)->bars[0].g2 = color->g >> 1;
                    VSUMMARY(dst)->bars[0].b2 = color->b;
                    color++;
                    VSUMMARY(dst)->bars[0].r3 = color->r >> 1;
                    VSUMMARY(dst)->bars[0].g3 = color->g >> 1;
                    VSUMMARY(dst)->bars[0].b3 = color->b;
                    color++;
                } else {
                    VSUMMARY(dst)->bars[0].r0 = color->r;
                    VSUMMARY(dst)->bars[0].g0 = color->g;
                    VSUMMARY(dst)->bars[0].b0 = color->b;
                    color++;
                    VSUMMARY(dst)->bars[0].r1 = color->r;
                    VSUMMARY(dst)->bars[0].g1 = color->g;
                    VSUMMARY(dst)->bars[0].b1 = color->b;
                    color++;
                    VSUMMARY(dst)->bars[0].r2 = color->r;
                    VSUMMARY(dst)->bars[0].g2 = color->g;
                    VSUMMARY(dst)->bars[0].b2 = color->b;
                    color++;
                    VSUMMARY(dst)->bars[0].r3 = color->r;
                    VSUMMARY(dst)->bars[0].g3 = color->g;
                    VSUMMARY(dst)->bars[0].b3 = color->b;
                    color++;
                }
                row_offset += 0xB;
                dst += sizeof(POLY_G4);
                i += 1;
                bar_offset += 0x24;
            } while (i < 3);
        }
        STATUS(status)->sprites[6].u0 = (s16)(identity->zodiac % 7) * 0x18;
        cell = identity->zodiac;
        STATUS(status)->sprites[6].w = 0x18;
        STATUS(status)->sprites[6].h = 0x14;
        STATUS(status)->sprites[6].v0 = (s16)(cell / 7) * 0x14 + 0x2A;
        if ((frame == 0) || (display->work != 0)) {
            world_gfx_copy_screen_setup_in(&g_world_selected_unit_stat_summary, &g_world_selected_unit_identity,
                &g_world_selected_unit_stat_detail);
            if (state->list_index < 0) {
                entries[6].param = 0xC00;
            } else {
                entries[6].param = 2;
            }
            world_clear_menu_render_buffer(render_a, 0x700);
            {
                s32* stride;
                s32* position;
                stride = &g_world_menu_text_state.stride;
                *stride = 0x38;
                if (g_world_preview_stats_window_active == 0) {
                    world_menu_draw_numeric_display_entries(
                        (s32)render_a, entries, (world_glyph_blit_t*)(stride - 2), 6);
                } else {
                    position = stride - 2;
                    world_text_render_decimal_entry_list((s32)render_a, &entries[10], (world_glyph_blit_t*)position, 1);
                    world_text_render_decimal_entry_list((s32)render_a, &entries[12], (world_glyph_blit_t*)position, 1);
                    world_menu_draw_numeric_display_entries(
                        (s32)render_a, &entries[11], (world_glyph_blit_t*)position, 1);
                    world_menu_draw_numeric_display_entries(
                        (s32)render_a, &entries[13], (world_glyph_blit_t*)position, 1);
                    world_menu_draw_numeric_display_entries(
                        (s32)render_a, &entries[4], (world_glyph_blit_t*)position, 2);
                }
            }
            LoadImage(&rects[0], (u32*)render_a);
            text_stride = &g_world_menu_text_state.stride;
            origin = text_stride - 2;
            render_a_upper = render_a + 0x400;
            *text_stride = 0x60;
            world_menu_draw_numeric_display_entries((s32)render_a_upper, &entries[8], (world_glyph_blit_t*)origin, 2);
            LoadImage(&rects[1], (u32*)render_a_upper);
            display->work = 0;
            world_clear_menu_render_buffer(render_c, 0x580);
            *text_stride = 0x58;
            world_menu_set_text_origin(0, 0);
            world_menu_display_text_entry(identity->unit_index + 0x4000, render_c, origin);
            world_menu_set_text_origin(0, 0x10);
            world_menu_display_text_entry(identity->job_id + 0x3000, render_c, origin);
            LoadImage(&rects[2], (u32*)render_c);
            world_clear_menu_render_buffer(render_b, 0xF0);
            *text_stride = 0x10;
            world_menu_set_text_origin(0, 0);
            world_text_render_decimal_value_or_dashes(identity->brave, 0x202, render_b, (world_glyph_blit_t*)origin);
            LoadImage(&rects[3], (u32*)render_b);
            world_menu_set_text_origin(0, 0);
            render_b_second = render_b + 0x50;
            world_text_render_decimal_value_or_dashes(
                identity->faith, 0x202, render_b_second, (world_glyph_blit_t*)origin);
            LoadImage(&rects[4], (u32*)render_b_second);
            if (g_world_unit_join_screen_active == 0) {
                world_menu_set_text_origin(0, 0);
                render_b_third = render_b + 0xA0;
                world_text_render_decimal_value_or_dashes(
                    identity->roster_slot + 1, 0x4002, render_b_third, (world_glyph_blit_t*)origin);
                LoadImage(&rects[5], (u32*)render_b_third);
            } else {
                LoadImage(&rects[5], (u32*)(render_b + 0xA0));
            }
        }
        if (!(display->flags & 0x80)) {
            world_gfx_draw_or_append_gpu_primitive(&SUMMARY(summary)->draw_offset_b);
            world_gfx_draw_or_append_gpu_primitive(&SUMMARY(summary)->value_sprites[0]);
            world_gfx_draw_or_append_gpu_primitive(&SUMMARY(summary)->value_sprites[1]);
            world_gfx_draw_or_append_gpu_primitive(&SUMMARY(summary)->draw_mode_a);
            i = 0;
            offset = 0x1E0;
            do {
                world_gfx_draw_or_append_gpu_primitive((summary + offset));
                i += 1;
                offset += 0x24;
            } while (i < 3);
            world_gfx_draw_or_append_gpu_primitive(&SUMMARY(summary)->value_sprites[2]);
            world_gfx_draw_or_append_gpu_primitive(&SUMMARY(summary)->value_sprites[3]);
            i = 0;
            offset = 0x154;
            do {
                world_gfx_draw_or_append_gpu_primitive((summary + offset));
                i += 1;
                offset += 0x14;
            } while (i < 7);
            world_gfx_draw_or_append_gpu_primitive(&SUMMARY(summary)->draw_mode_b);
            if (suppress == 0) {
                if (anim_state >= 5) {
                    world_gfx_draw_or_append_gpu_primitive(&SUMMARY(summary)->portrait[2]);
                    world_gfx_draw_or_append_gpu_primitive(&SUMMARY(summary)->portrait[3]);
                    world_gfx_draw_or_append_gpu_primitive(&SUMMARY(summary)->portrait[1]);
                }
                world_gfx_draw_or_append_gpu_primitive(&SUMMARY(summary)->portrait[0]);
            }
            world_gfx_submit_primitive_group((world_primitive_group_t*)summary);
            world_gfx_draw_or_append_gpu_primitive(&SUMMARY(summary)->draw_offset_a);
            world_gfx_draw_or_append_gpu_primitive(&STATUS(status)->draw_offset_b);
            portrait_prim = (u8*)&STATUS(status)->portrait;
            SetSemiTrans(portrait_prim, 1);
            world_gfx_draw_or_append_gpu_primitive(portrait_prim);
            i = 3;
            offset = 0x128;
            do {
                world_gfx_draw_or_append_gpu_primitive((status + offset));
                i -= 1;
                offset -= 0x14;
            } while (i >= 0);
            world_gfx_draw_or_append_gpu_primitive(&STATUS(status)->draw_mode_b);
            i = 4;
            offset = 0x13C;
            do {
                world_gfx_draw_or_append_gpu_primitive((status + offset));
                i += 1;
                offset += 0x14;
            } while (i < 7);
            world_gfx_draw_or_append_gpu_primitive(&STATUS(status)->draw_mode_a);
            world_gfx_submit_primitive_group((world_primitive_group_t*)status);
            world_gfx_draw_or_append_gpu_primitive(&STATUS(status)->draw_offset_a);
        }
        world_thread_yield();
        if (world_thread_get_current_parameter_3() != 0) {
            break;
        }
        frame += 1;
    }
    world_thread_yield();
    world_gfx_free_texture_grid_rect(&rects[0]);
    world_gfx_free_texture_grid_rect(&rects[1]);
    world_gfx_free_texture_grid_rect(&rects[2]);
    world_gfx_free_texture_grid_rect(&rects[3]);
    world_gfx_free_texture_grid_rect(&rects[4]);
    world_gfx_free_texture_grid_rect(&rects[5]);
    world_thread_exit_current();
}
