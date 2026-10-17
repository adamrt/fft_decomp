/* The unit status thread, text images at a fixed origin, message box slots and typewriter glyphs, quad flips,
 * fixed-point lerps and window quad animations. */

#include "fft/message_box.h"
#include "fft/formation.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_window.h"
#include "fft/option.h"
#include "fft/portrait.h"
#include "fft/screen.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/status_screen.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* message glyphs and staging slots */
extern glyph_blit_t g_message_box_state;
extern text_glyph_state_t g_message_box_glyph;
extern s32 g_message_box_glyph_delay;
extern RECT g_message_box_vram_rect;
extern s32 g_message_box_slot_threads[3];
extern s32 g_message_box_slot_glyph_counters[3];
extern u8 g_message_box_slot_carry_offset[4];
extern u8 g_message_box_slot_prev_width[4];
extern u8 g_message_box_glyph_images[3][8][84];
extern u8 g_message_box_previous_images[3][84];
extern u8 g_message_box_column_scratch[14];
extern s32 g_message_box_digit_divisors[];

static u16 g_formation_summary_value_cluts[4][2]
    = { 0x7ebd, 0x7efd, 0x7ebd, 0x7efd, 0x7ebd, 0x7efd, 0x7ebd, 0x7efd }; /* [row][dimmed] */

static u16 g_formation_summary_label_cluts[7][2] = { 0x7efc, 0x7efc, 0x7f3c, 0x7f3c, 0x7f7c, 0x7f7c, 0x7ebd, 0x7efd,
    0x7ebd, 0x7efd, 0x7ebd, 0x7efd, 0x7ebd, 0x7efd }; /* [row][dimmed] */
static image_layout_t g_formation_summary_mode_row_params[4] = { { { 0, 0, 2, 2 }, 0x200, 0 },
    { { 228, 144, 28, 10 }, -2, -4 }, { { 204, 144, 24, 10 }, -1, -3 }, { { 232, 64, 24, 8 }, 0, -4 } };
static RECT g_formation_selected_unit_portrait_rect = { 0, 510, 16, 1 };
static RECT g_formation_comparison_unit_portrait_rect = { 32, 510, 16, 1 };

/*
 * Thread task THREAD_TASK_UNIT_STATUS_BANNER: the unit status banner
 * (portrait, name and job, level, exp, HP/MP/CT bars, Brave, Faith and
 * zodiac). WORLD's body.
 *
 * The unit status slot draws the selected unit's banner and any other slot the comparison
 * unit's; each has its own packets, text images and unit records. The thread's
 * first parameter is its unit_panel_thread_t. Both
 * packet sets are built once and copied for the second frame buffer, then
 * every frame the thread:
 *  - dims the banner while a THREAD_TASK_STATUS_PANEL thread runs with a text
 *    task active, or the display record asks for it;
 *  - runs the slide-down or slide-up table once when the display flags ask
 *    for it, leaving only MENU_STATUS_PANEL_HIDDEN set at the end;
 *  - cross-fades the portrait when the shown unit changes
 *    (portrait_build_transition);
 *  - redraws the number and name images on the first frame and whenever
 *    display->work is set. While the preview-stats window is open the first
 *    number block is drawn by five separate calls.
 * The gauge bars have a fixed width here. On the unit-join screen the mode
 * row is always row 0 and the roster number is left blank. The thread runs
 * until its third parameter is set, then frees its six text images.
 */
void unit_panel_banner_thread(void) {
    RECT rects[6];
    s32 anim_state;
    s32 cur_unit;
    s32 prev_unit;

    u8* number_pixels;                              /* sp60 */
    menu_status_panel_editor_packet_t* editor_base; /* sp68 */
    menu_number_entry_t* number_entries;            /* sp70 */
    unit_panel_summary_t* state;                    /* sp78 */
    s32 portrait_arg;                               /* sp80 */
    const RECT* transition;                         /* sp88 */
    palette_t* portrait_palettes;                   /* sp90 */
    menu_unit_number_images_t* number_images;       /* sp98 */
    u8* name_image;                                 /* spA0 */
    menu_status_panel_packet_t* panel_base;         /* spA8 */
    menu_unit_status_identity_t* identity;          /* spB0 */
    unit_panel_thread_t* thread;                    /* spB8 */
    s32 frame;                                      /* spC0 */
    s32 shake;                                      /* spC8 */
    s32 dimmed;                                     /* spD0 */
    s32 suppress;                                   /* spD8 */

    s32 thread_id;
    s32 i;
    s32 draw_x;
    s32 draw_y;
    s16 cell;
    menu_status_panel_packet_t* panel;
    menu_status_panel_editor_packet_t* editor;
    u32* input;
    u8* stat_image_upper;
    POLY_FT4* panel_portrait;

    suppress = 0;
    thread_set_current_task_id(THREAD_TASK_UNIT_STATUS_BANNER);
    input = script_get_controller_input_pointer(0);
    thread_id = g_thread_current_id;
    g_menu_input_controller = input + 1;
    thread = (unit_panel_thread_t*)g_threads[thread_id].function_parameter_1;

    if (thread_id == THREAD_SCREEN_SLOT_UNIT_STATUS) {
        panel = g_formation_selected_unit_status_packets;
        editor = g_formation_selected_unit_summary_packets;
        number_pixels = g_formation_selected_unit_stat_text_image;
        number_images = &g_formation_selected_unit_number_text_images;
        name_image = g_formation_selected_unit_name_text_image;
        number_entries = g_formation_selected_unit_number_entries;
        state = &g_unit_panel_selected_unit_stat_summary;
        identity = &g_formation_selected_unit_identity;
        transition = &g_formation_selected_unit_portrait_rect;
        panel_base = panel;
        editor_base = editor;
        portrait_palettes = g_formation_selected_unit_portrait_palettes;
    } else {
        panel = g_formation_comparison_unit_status_packets;
        editor = g_formation_comparison_unit_summary_packets;
        number_pixels = g_formation_comparison_unit_stat_text_image;
        number_images = &g_formation_comparison_unit_number_text_images;
        name_image = g_formation_comparison_unit_name_text_image;
        number_entries = g_formation_comparison_unit_number_entries;
        state = &g_formation_comparison_unit_stat_summary;
        identity = &g_formation_comparison_unit_identity;
        transition = &g_formation_comparison_unit_portrait_rect;
        panel_base = panel;
        editor_base = editor;
        portrait_palettes = g_formation_comparison_unit_portrait_palettes;
    }
    shake = 0;

    /* Build the summary packets once, then copy them for the second frame buffer. */
    status_screen_init_numbers_frame(&g_menu_portrait_origin, &editor->numeric_frame);
    gfx_set_draw_mode_for_texture_page(&editor->draw_mode_a, MENU_TEXTURE_PAGE_ALLOCATED_IMAGES);
    gfx_set_draw_mode_for_texture_page(&editor->draw_mode_b, MENU_TEXTURE_PAGE_FRAME);
    menu_init_sprite_array(&editor->label_sprites[0], 7, 0x7CBC);
    menu_init_sprite_array(&editor->value_sprites[0], 4, 0x7CBC);
    for (frame = 0; frame < 4; frame++) {
        menu_init_default_poly_ft4(&editor->portrait[frame]);
    }
    text_configure_sprite_vram(&rects[0], 0x38, 0x28, &editor->value_sprites[0], 2);
    text_configure_sprite_vram(&rects[1], 0x60, 0x10, &editor->value_sprites[1], 2);
    for (frame = 0; frame < 4; frame++) {
        gfx_set_image_primitive_layout(&editor->value_sprites[frame], &g_unit_panel_texture_origin,
            &g_menu_portrait_origin, &g_formation_summary_value_sprite_params[frame]);
    }

    /* Label 6 is the mode row, chosen by team kind; the unit-join screen always uses row 0. */
    if (state->team_kind >= 4) {
        state->team_kind = 0;
    }
    if (g_menu_post_battle_unit_changes_active == 0) {
        mem_copy_bytes_nonempty(&g_formation_summary_label_sprite_params[6],
            &g_formation_summary_mode_row_params[state->team_kind], sizeof(image_layout_t));
    } else {
        mem_copy_bytes_nonempty(
            &g_formation_summary_label_sprite_params[6], g_formation_summary_mode_row_params, sizeof(image_layout_t));
    }
    for (frame = 0; frame < 7; frame++) {
        gfx_set_image_primitive_layout(&editor->label_sprites[frame], &g_unit_panel_texture_origin,
            &g_menu_portrait_origin, &g_formation_summary_label_sprite_params[frame]);
    }
    mem_copy_bytes_nonempty(editor + 1, editor, sizeof(menu_status_panel_editor_packet_t));

    /* The same for the status panel packets. */
    gfx_set_draw_mode_for_texture_page(&panel->draw_mode_a, MENU_TEXTURE_PAGE_FRAME);
    gfx_set_draw_mode_for_texture_page(&panel->draw_mode_b, MENU_TEXTURE_PAGE_ALLOCATED_IMAGES);
    status_screen_init_numbers_frame(&g_formation_status_panel_origin, &panel->numeric_frame);
    menu_init_sprite_array(panel->sprites, 7, MENU_CLUT_NORMAL);
    menu_init_default_poly_ft4(&panel->portrait);
    text_configure_sprite_vram(&rects[2], 0x58, 0x20, panel->sprites, 0);
    text_configure_sprite_vram(&rects[3], 0x10, 0xA, &panel->sprites[1], 0);
    text_configure_sprite_vram(&rects[4], 0x10, 0xA, &panel->sprites[2], 0);
    text_configure_sprite_vram(&rects[5], 0x10, 0xA, &panel->sprites[3], 0);
    for (frame = 0; frame < 7; frame++) {
        gfx_set_image_primitive_layout(&panel->sprites[frame], &g_unit_panel_texture_origin,
            &g_formation_status_panel_origin, &g_formation_status_sprite_params[frame]);
    }
    gfx_set_image_primitive_layout(&panel->portrait, &g_unit_panel_texture_origin, &g_formation_status_panel_origin,
        &g_formation_status_portrait_params);
    if (state->team_kind == 1) {
        panel->portrait.clut = 0x7FFD;
    } else {
        panel->portrait.clut = 0x7FBD;
    }
    panel->portrait.tpage = GetTPage(0, 1, 0x3C0, 0x100);
    mem_copy_bytes_nonempty(panel + 1, panel, sizeof(menu_status_panel_packet_t));

    frame = 0;
    anim_state = 0;
    prev_unit = state->unit_index;
    cur_unit = state->unit_index;
    for (;;) {
        /* The two packet copies alternate frame by frame. */
        s32 parity = frame & 1;
        panel = panel_base + parity;
        editor = editor_base + parity;

        if (state->gauges[MENU_UNIT_STATUS_GAUGE_CT].value > UNIT_TURN_CT_THRESHOLD) {
            state->gauges[MENU_UNIT_STATUS_GAUGE_CT].value = UNIT_TURN_CT_THRESHOLD;
        }
        if (g_menu_post_battle_unit_changes_active == 0) {
            mem_copy_bytes_nonempty(&g_formation_summary_label_sprite_params[6],
                &g_formation_summary_mode_row_params[state->team_kind], sizeof(image_layout_t));
        } else {
            mem_copy_bytes_nonempty(&g_formation_summary_label_sprite_params[6], g_formation_summary_mode_row_params,
                sizeof(image_layout_t));
        }
        gfx_set_image_primitive_layout(&editor->label_sprites[6], &g_unit_panel_texture_origin, &g_menu_portrait_origin,
            &g_formation_summary_label_sprite_params[6]);
        if (state->team_kind == 1) {
            panel->portrait.clut = 0x7FFD;
        } else {
            panel->portrait.clut = 0x7FBD;
        }

        /* Dim while a status panel thread runs under a menu input lock, or when the display asks. */
        dimmed = 0;
        if (thread_find_running_by_task(THREAD_TASK_STATUS_PANEL) != 0) {
            dimmed = g_menu_input_lock_mode != 0;
        }
        if (thread->style != 0) {
            dimmed = 1;
        }
        for (i = 0; i < 4; i++) {
            editor->value_sprites[i].clut = g_formation_summary_value_cluts[i][dimmed];
        }
        for (i = 0; i < 7; i++) {
            editor->label_sprites[i].clut = g_formation_summary_label_cluts[i][dimmed];
        }
        for (i = 0; i < 7; i++) {
            if (dimmed != 0) {
                panel->sprites[i].clut = MENU_CLUT_DIMMED;
            } else {
                panel->sprites[i].clut = MENU_CLUT_NORMAL;
            }
        }

        /* During cross-fade steps 4-9 portrait quad 0 is drawn at half its usual brightness. */
        if (dimmed != 0) {
            menu_init_primitive_colors_dimmed(&editor->numeric_frame);
            menu_init_primitive_colors_dimmed(&panel->numeric_frame);
            if ((anim_state - 4) < 6U) {
                setRGB0(&editor->portrait[0], 0x20, 0x28, 0x38);
            } else {
                setRGB0(&editor->portrait[0], 0x40, 0x50, 0x70);
            }
            setRGB0(&editor->portrait[1], 0x20, 0x28, 0x38);
            setRGB0(&editor->portrait[2], 0x20, 0x28, 0x38);
            setRGB0(&editor->portrait[3], 0x20, 0x28, 0x38);
        } else {
            s32 shade;
            menu_init_primitive_colors_normal(&editor->numeric_frame);
            menu_init_primitive_colors_normal(&panel->numeric_frame);
            if ((anim_state - 4) >= 6U) {
                setRGB0(&editor->portrait[0], 0x80, 0x80, 0x80);
            } else {
                setRGB0(&editor->portrait[0], 0x40, 0x40, 0x40);
            }
            shade = 0x40;
            setRGB0(&editor->portrait[1], shade, shade, shade);
            setRGB0(&editor->portrait[2], shade, shade, shade);
            setRGB0(&editor->portrait[3], shade, shade, shade);
        }
        if (dimmed != 0) {
            setRGB0(&panel->portrait, 0x40, 0x40, 0x60);
        } else {
            setRGB0(&panel->portrait, 0x80, 0x80, 0x80);
        }

        g_gfx_draw_offset_y = (g_frame_arg != 0xF0) ? 0xF0 : 0;

        /* Slide down through steps 1-6 or up through 11-16, one step every two frames, then keep
         * only the hidden flag. */
        if ((thread->flags & MENU_STATUS_PANEL_SLIDE_DOWN) && (shake == 0)) {
            shake = 1;
        }
        if (thread->flags & MENU_STATUS_PANEL_SLIDE_UP) {
            if (shake == 0) {
                shake = 0xB;
            }
        }
        if (shake != 0) {
            if (shake < 0xA) {
                thread->position.y = g_unit_panel_slide_y[0][shake - 1];
                if (frame & 1) {
                    shake += 1;
                }
                if (shake >= 7) {
                    shake = 0;
                    thread->flags = thread->flags & MENU_STATUS_PANEL_HIDDEN;
                }
            } else if (shake >= 0xB) {
                thread->position.y = g_unit_panel_slide_y[1][shake - 11];
                if (frame & 1) {
                    shake += 1;
                }
                if (shake >= 0x11) {
                    shake = 0;
                    thread->flags = thread->flags & MENU_STATUS_PANEL_HIDDEN;
                }
            }
        }

        draw_x = (u16)thread->position.x;
        draw_y = g_gfx_draw_offset_y;
        panel->draw_offset_a.position.x = draw_x - 0x80;
        panel->draw_offset_a.position.y = thread->position.y + draw_y;
        SetDrawOffset(&panel->draw_offset_a.packet, (u16*)&panel->draw_offset_a.position);
        panel->draw_offset_b.position.x = -0x80;
        panel->draw_offset_b.position.y = g_gfx_draw_offset_y;
        SetDrawOffset(&panel->draw_offset_b.packet, (u16*)&panel->draw_offset_b.position);
        draw_x = (u16)thread->position.x;
        draw_y = g_gfx_draw_offset_y;
        editor->draw_offset_a.position.x = draw_x - 0x80;
        editor->draw_offset_a.position.y = thread->position.y + draw_y;
        SetDrawOffset(&editor->draw_offset_a.packet, (u16*)&editor->draw_offset_a.position);
        editor->draw_offset_b.position.x = -0x80;
        editor->draw_offset_b.position.y = g_gfx_draw_offset_y;
        SetDrawOffset(&editor->draw_offset_b.packet, (u16*)&editor->draw_offset_b.position);

        /* A finished cross-fade (step 10) settles on the new unit; a changed unit, or a redraw
         * request arriving mid-fade, (re)starts the fade at step 2. */
        if (anim_state >= 0xA) {
            anim_state = 0;
            prev_unit = cur_unit;
            suppress = thread->flags & MENU_STATUS_PANEL_HIDDEN;
        }
        if (thread->redraw_request != 0) {
            if (anim_state == 0) {
                prev_unit = cur_unit;
                portrait_arg = thread->redraw_request;
                cur_unit = state->unit_index;
            } else {
                anim_state = 2;
                prev_unit = cur_unit;
                portrait_arg = thread->redraw_request;
                cur_unit = state->unit_index;
            }
        }
        if (cur_unit != prev_unit) {
            if (anim_state == 0) {
                anim_state = 2;
            }
        }
        portrait_build_transition(
            transition, &anim_state, &cur_unit, &prev_unit, portrait_palettes, &editor->portrait[0], portrait_arg);

        /* The three gauge bars have a fixed width and a 3-pixel slant; dimming halves red and green. */
        {
            CVECTOR* color;

            color = g_formation_panel_bar_colors;
            for (i = 0; i < 3; i++) {
                s32 x0;
                s32 y;

                SetPolyG4(&editor->bars[i]);
                x0 = g_menu_portrait_origin.x + 0x2F;
                editor->bars[i].x0 = x0;
                y = i * 0xB + 0x18;
                editor->bars[i].y0 = g_menu_portrait_origin.y + y;
                editor->bars[i].x1 = x0 + 0x20;
                editor->bars[i].y1 = g_menu_portrait_origin.y + y;
                editor->bars[i].x2 = x0;
                editor->bars[i].y2 = g_menu_portrait_origin.y + y + 3;
                editor->bars[i].x3 = x0 + 0x20;
                editor->bars[i].y3 = g_menu_portrait_origin.y + y + 3;
                editor->bars[i].x2 -= 3;
                editor->bars[i].x3 -= 3;
                if (dimmed != 0) {
                    setRGB0(&editor->bars[i], color->r >> 1, color->g >> 1, color->b);
                    color++;
                    setRGB1(&editor->bars[i], color->r >> 1, color->g >> 1, color->b);
                    color++;
                    setRGB2(&editor->bars[i], color->r >> 1, color->g >> 1, color->b);
                    color++;
                    setRGB3(&editor->bars[i], color->r >> 1, color->g >> 1, color->b);
                    color++;
                } else {
                    setRGB0(&editor->bars[i], color->r, color->g, color->b);
                    color++;
                    setRGB1(&editor->bars[i], color->r, color->g, color->b);
                    color++;
                    setRGB2(&editor->bars[i], color->r, color->g, color->b);
                    color++;
                    setRGB3(&editor->bars[i], color->r, color->g, color->b);
                    color++;
                }
            }
        }

        /* Zodiac icons sit 7 to a row, each 0x18 by 0x14. */
        panel->sprites[6].u0 = (identity->zodiac % 7) * 0x18;
        cell = identity->zodiac;
        panel->sprites[6].w = 0x18;
        panel->sprites[6].h = 0x14;
        panel->sprites[6].v0 = (cell / 7) * 0x14 + 0x2A;

        if ((frame == 0) || (thread->redraw_request != 0)) {
            /* WORLD keeps its identity view (unit index, roster slot) in the shared record. */
            unit_panel_copy_billboard_in(&g_unit_panel_selected_unit_stat_summary,
                (const unit_panel_unit_t*)&g_formation_selected_unit_identity, &g_formation_selected_unit_stat_detail);
            if (state->list_index < 0) {
                number_entries[6].format = 0xC00;
            } else {
                number_entries[6].format = 2;
            }
            menu_clear_render_buffer(number_pixels, 0x700);
            {
                g_menu_window_text_pen.stride = 0x38;
                if (g_formation_preview_stats_window_active == 0) {
                    menu_draw_numeric_display_entries(number_pixels, number_entries, &g_menu_window_text_pen, 6);
                } else {
                    menu_text_render_decimal_entry_list(number_pixels, &number_entries[10], &g_menu_window_text_pen, 1);
                    menu_text_render_decimal_entry_list(number_pixels, &number_entries[12], &g_menu_window_text_pen, 1);
                    menu_draw_numeric_display_entries(number_pixels, &number_entries[11], &g_menu_window_text_pen, 1);
                    menu_draw_numeric_display_entries(number_pixels, &number_entries[13], &g_menu_window_text_pen, 1);
                    menu_draw_numeric_display_entries(number_pixels, &number_entries[4], &g_menu_window_text_pen, 2);
                }
            }
            LoadImage(&rects[0], (u32*)number_pixels);
            stat_image_upper = number_pixels + 0x400;
            g_menu_window_text_pen.stride = 0x60;
            menu_draw_numeric_display_entries(stat_image_upper, &number_entries[8], &g_menu_window_text_pen, 2);
            LoadImage(&rects[1], (u32*)stat_image_upper);
            thread->redraw_request = 0;

            /* Name and job, then the large Brave, Faith and roster numbers. */
            menu_clear_render_buffer(name_image, 0x580);
            g_menu_window_text_pen.stride = 0x58;
            menu_set_text_origin(0, 0);
            menu_display_text_entry(identity->unit_index + TEXT_ID_UNIT_NAME_BASE, name_image, &g_menu_window_text_pen);
            menu_set_text_origin(0, 0x10);
            menu_display_text_entry(identity->job_id + TEXT_ID_JOB_NAME_BASE, name_image, &g_menu_window_text_pen);
            LoadImage(&rects[2], (u32*)name_image);

            menu_clear_render_buffer(number_images, sizeof(*number_images));
            g_menu_window_text_pen.stride = 0x10;
            menu_set_text_origin(0, 0);
            text_draw_small_number_glyphs(
                identity->brave, SMALL_NUMBER_WIDEN_FOR_100 | 2, number_images->brave, &g_menu_window_text_pen);
            LoadImage(&rects[3], (u32*)number_images->brave);
            menu_set_text_origin(0, 0);
            text_draw_small_number_glyphs(
                identity->faith, SMALL_NUMBER_WIDEN_FOR_100 | 2, number_images->faith, &g_menu_window_text_pen);
            LoadImage(&rects[4], (u32*)number_images->faith);
            /* The unit-join screen uploads the roster number image cleared. */
            if (g_menu_post_battle_unit_changes_active == 0) {
                menu_set_text_origin(0, 0);
                text_draw_small_number_glyphs(
                    identity->roster_slot + 1, 0x4002, number_images->roster_slot, &g_menu_window_text_pen);
                LoadImage(&rects[5], (u32*)number_images->roster_slot);
            } else {
                LoadImage(&rects[5], (u32*)number_images->roster_slot);
            }
        }

        /* Queue the summary packets, then the status panel's; the other portrait quads show
         * from cross-fade step 5. */
        if (!(thread->flags & MENU_STATUS_PANEL_HIDDEN)) {
            gfx_draw_or_append_gpu_primitive(&editor->draw_offset_b);
            gfx_draw_or_append_gpu_primitive(&editor->value_sprites[0]);
            gfx_draw_or_append_gpu_primitive(&editor->value_sprites[1]);
            gfx_draw_or_append_gpu_primitive(&editor->draw_mode_a);
            for (i = 0; i < 3; i++) {
                gfx_draw_or_append_gpu_primitive(&editor->bars[i]);
            }
            gfx_draw_or_append_gpu_primitive(&editor->value_sprites[2]);
            gfx_draw_or_append_gpu_primitive(&editor->value_sprites[3]);
            for (i = 0; i < 7; i++) {
                gfx_draw_or_append_gpu_primitive(&editor->label_sprites[i]);
            }
            gfx_draw_or_append_gpu_primitive(&editor->draw_mode_b);
            if (suppress == 0) {
                if (anim_state >= 5) {
                    gfx_draw_or_append_gpu_primitive(&editor->portrait[2]);
                    gfx_draw_or_append_gpu_primitive(&editor->portrait[3]);
                    gfx_draw_or_append_gpu_primitive(&editor->portrait[1]);
                }
                gfx_draw_or_append_gpu_primitive(&editor->portrait[0]);
            }
            status_screen_submit_numbers_frame(&editor->numeric_frame);
            gfx_draw_or_append_gpu_primitive(&editor->draw_offset_a);
            gfx_draw_or_append_gpu_primitive(&panel->draw_offset_b);
            panel_portrait = &panel->portrait;
            SetSemiTrans(panel_portrait, 1);
            gfx_draw_or_append_gpu_primitive(panel_portrait);
            for (i = 3; i >= 0; i--) {
                gfx_draw_or_append_gpu_primitive(&panel->sprites[i]);
            }
            gfx_draw_or_append_gpu_primitive(&panel->draw_mode_b);
            for (i = 4; i < 7; i++) {
                gfx_draw_or_append_gpu_primitive(&panel->sprites[i]);
            }
            gfx_draw_or_append_gpu_primitive(&panel->draw_mode_a);
            status_screen_submit_numbers_frame(&panel->numeric_frame);
            gfx_draw_or_append_gpu_primitive(&panel->draw_offset_a);
        }

        thread_yield();
        if (thread_get_current_parameter_3() != 0) {
            break;
        }
        frame += 1;
    }

    /* Parameter 3 ends the thread: free the six text images. */
    thread_yield();
    gfx_free_texture_rect(&rects[0]);
    gfx_free_texture_rect(&rects[1]);
    gfx_free_texture_rect(&rects[2]);
    gfx_free_texture_rect(&rects[3]);
    gfx_free_texture_rect(&rects[4]);
    gfx_free_texture_rect(&rects[5]);
    thread_exit_current();
}

/* Render a message box's lines into its window image with the pen at (8, 8) and upload it; a
 * portrait box builds its tailed window image first, and centered boxes are skipped. */
static void message_box_render_text_image(text_dialog_record_t* record) {
    if (record->box_type != DIALOG_BOX_TYPE_CENTERED) {
        if (record->box_type == DIALOG_BOX_TYPE_PORTRAIT) {
            record->buffer = menu_build_window_image(
                record->width, record->height, &record->rect, record->dialog_type, record->tail_offset, 1);
        } else {
            record->buffer = menu_build_and_upload_window_frame_image(
                record->width, record->height, &record->rect, MENU_WINDOW_FRAME_BUILD);
        }
        g_menu_window_text_pen.stride = record->rect.w * 4;
        g_menu_window_text_pen.rect.x = 8;
        g_menu_window_text_pen.rect.y = 8;
        g_menu_text_first_line = record->first_line;
        g_menu_text_line_limit = record->last_line;
        menu_display_text_entry(record->text, record->buffer, &g_menu_window_text_pen);
        LoadImage(&record->rect, record->buffer);
        thread_yield();
        menu_free_memory(record->buffer);
    }
}

/* Free the message box text slot owned by the current thread, if any, and reset its glyph counter. */
static void message_box_release_slot(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (g_thread_current_id == g_message_box_slot_threads[i]) {
            g_message_box_slot_threads[i] = 0;
            g_message_box_slot_glyph_counters[i] = 0;
            return;
        }
    }
}

/* Draw the message box's pending glyph into VRAM and hold it for the typewriter delay.
 *
 * Cousin of message_draw_glyph_with_delay without the request pointer, task-id checks or
 * script counters. The ring byte is read before the phase and the inner bound
 * is `width / 2`, which keeps the per-row recomputation of the target.
 */
static void message_box_draw_glyph(s32 x, s32 y, s32 wait) {
    s32 slot;
    s32 i;
    s32 j;
    s32 phase;
    u8* dst;
    u8* src;
    s32 width;
    u8 ring;
    s32 offset;

    /* Only portrait, Check and centered (mode 0) boxes type glyph by glyph; the others just yield. */
    if (g_message_box_glyph.mode != DIALOG_BOX_TYPE_PORTRAIT && g_message_box_glyph.mode != DIALOG_BOX_TYPE_CHECK
        && g_message_box_glyph.mode != 0) {
        thread_yield();
        return;
    }

    /* Find this thread's slot or claim a free one; with all three taken, wait a frame and retry. */
    for (;;) {
        slot = 0x10;
        for (i = 0; i < 3; i++) {
            if (g_thread_current_id == g_message_box_slot_threads[i]) {
                slot = i;
                g_message_box_slot_threads[i] = g_thread_current_id;
                break;
            }
        }
        if (i == 3) {
            for (j = 0; j < 3; j++) {
                if (g_message_box_slot_threads[j] == 0) {
                    slot = j;
                    g_message_box_slot_threads[j] = g_thread_current_id;
                    g_message_box_slot_carry_offset[slot] = 0;
                    g_message_box_slot_prev_width[slot] = 0;
                    break;
                }
            }
        }
        if (slot != 0x10) {
            break;
        }
        thread_yield();
    }

    ring = g_message_box_slot_glyph_counters[slot];
    phase = g_message_box_glyph.x & 3;
    dst = g_message_box_glyph_images[slot][ring];
    src = g_message_box_previous_images[slot];

    /* VRAM x counts 4-pixel words: upload enough of them to cover the glyph from its phase in the first. */
    if (g_text_glyph_widths[g_message_box_glyph.glyph] == 4 || g_message_box_glyph.glyph == TEXT_SPACE) {
        if (phase == 2) {
            width = 8;
            g_message_box_vram_rect.w = 2;
        } else {
            width = 4;
            g_message_box_vram_rect.w = 1;
        }
    } else if (g_text_glyph_widths[g_message_box_glyph.glyph] == 2) {
        width = 4;
        g_message_box_vram_rect.w = 1;
    } else if (g_text_glyph_widths[g_message_box_glyph.glyph] == 6) {
        width = 8;
        g_message_box_vram_rect.w = 2;
    } else {
        width = 12;
        g_message_box_vram_rect.w = 3;
    }

    /* At phase 2 the first word is shared with the previous glyph: carry its byte over. */
    if (phase == 2) {
        for (i = 0; i < 14; i++) {
            g_message_box_column_scratch[i]
                = *(src + (i * g_message_box_slot_prev_width[slot]) / 2 + g_message_box_slot_carry_offset[slot]);
        }
        for (i = 0; i < 14; i++) {
            dst[i * width / 2] = g_message_box_column_scratch[i];
        }
    }

    /* Record which byte of this image the next glyph will carry over. */
    if (g_text_glyph_widths[g_message_box_glyph.glyph] == 4 || g_message_box_glyph.glyph == TEXT_SPACE) {
        if (phase == 2) {
            g_message_box_slot_carry_offset[slot] = 2;
        } else {
            g_message_box_slot_carry_offset[slot] = 1;
        }
    } else if (g_text_glyph_widths[g_message_box_glyph.glyph] == 2) {
        if (phase == 2) {
            g_message_box_slot_carry_offset[slot] = 1;
        } else {
            g_message_box_slot_carry_offset[slot] = 0;
        }
    } else if (g_text_glyph_widths[g_message_box_glyph.glyph] == 6) {
        if (phase == 2) {
            g_message_box_slot_carry_offset[slot] = 3;
        } else {
            g_message_box_slot_carry_offset[slot] = 2;
        }
    } else {
        if (phase == 2) {
            g_message_box_slot_carry_offset[slot] = 5;
        } else {
            g_message_box_slot_carry_offset[slot] = 4;
        }
    }
    g_message_box_slot_prev_width[slot] = width;

    /* Fill the rest of the image with background pixels from the glyph sheet, or zero in mode 0. */
    for (j = 0; j < 14; j++) {
        offset = j * width;
        for (i = phase >> 1; i < width / 2; i++) {
            if (g_message_box_glyph.mode == 0) {
                (dst + i)[offset / 2] = 0;
            } else {
                (dst + i)[offset / 2] = (g_menu_glyph_sheet->pixels
                    + ((((g_message_box_glyph.x + (i << 1)) & 0xF) + 8) / 2))[(j << 7) + 0x400];
            }
        }
    }

    if (g_message_box_glyph.glyph != TEXT_SPACE) {
        g_message_box_state.rect.x = phase;
        g_message_box_state.rect.y = 0;
        g_message_box_state.stride = width;
        text_render_glyph_to_4bpp_image(g_text_glyph_bitmap_data + g_message_box_glyph.glyph * TEXT_GLYPH_BYTES, dst,
            &g_message_box_state, g_message_box_glyph.palette);
        g_message_box_vram_rect.x = (x >> 2) + 0x1c0;
        g_message_box_vram_rect.y = y;
        g_message_box_vram_rect.h = 14;
        LoadImage(&g_message_box_vram_rect, (u32*)dst);
    }
    /* Keep this image (14 rows of up to 12 pixels) for the next glyph's carry. */
    g_message_box_slot_glyph_counters[slot]++;
    mem_copy_bytes_nonempty(src, dst, 84);

    /* Wait 0 only syncs; 2 or more waits that many steps; 1 pauses after every few glyphs as the
     * message speed option sets (holding Circle counts as fast). */
    if (wait == 0) {
        DrawSync(0);
        g_message_box_slot_glyph_counters[slot] = 0;
    } else if (wait >= 2) {
        for (i = 0; i < wait * (3 - g_script_event_speed); i++) {
            thread_wait_frames(1);
        }
        g_message_box_slot_glyph_counters[slot] = 0;
    } else {
        if (PadRead(1) & PSX_PAD_CIRCLE) {
            g_message_box_glyph_delay = 8;
        } else if (g_game_options.fields.message_display_speed == GAME_MESSAGE_DISPLAY_SPEED_FAST) {
            g_message_box_glyph_delay = 8;
        } else if (g_game_options.fields.message_display_speed == GAME_MESSAGE_DISPLAY_SPEED_REGULAR) {
            g_message_box_glyph_delay = 2;
        } else {
            g_message_box_glyph_delay = 1;
        }
        if (g_message_box_slot_glyph_counters[slot] >= g_message_box_glyph_delay) {
            for (i = 0; i < 3 - g_script_event_speed; i++) {
                thread_wait_frames(1);
            }
            g_message_box_slot_glyph_counters[slot] = 0;
        }
    }

    if (g_message_box_glyph.mode != 0) {
        g_sound_pending_sfx_id = SOUND_SFX_TEXT_GLYPH;
    }
}

/* Mirror a POLY_FT4 texture horizontally when direction is negative: u0/u1
 * and u2/u3 trade places, each shifted by one texel. */
static void screen_flip_poly_ft4_u(POLY_FT4* poly, s32 direction) {
    s32 step;
    u8 u;

    if (direction >= 0) {
        return;
    }
    if (poly->u0 < poly->u1) {
        step = -1;
    } else {
        step = 1;
    }
    u = poly->u0 + step;
    poly->u0 = poly->u1 + step;
    poly->u1 = u;
    u = poly->u2 + step;
    poly->u2 = poly->u3 + step;
    poly->u3 = u;
}

/* The menu-screen library's copy of menu_lerp_q12 (the same routine compiled twice). */
static s32 screen_lerp_q12(s32 from, s32 to, s32 t) {
    return ((to - from) * t) / ONE + from;
}

/* Interpolate the four corners of a textured quad between two RECTs. The menu-screen library's copy of
 * menu_lerp_rect_to_poly_ft4; screen_animate_window_quads_to_rects calls this one. */
static void screen_lerp_rect_to_poly_ft4(POLY_FT4* poly, const RECT* from, const RECT* to, s32 t) {
    setXY4(poly, screen_lerp_q12(from->x, to->x, t), screen_lerp_q12(from->y, to->y, t),
        screen_lerp_q12(from->x + from->w, to->x + to->w, t), screen_lerp_q12(from->y, to->y, t),
        screen_lerp_q12(from->x, to->x, t), screen_lerp_q12(from->y + from->h, to->y + to->h, t),
        screen_lerp_q12(from->x + from->w, to->x + to->w, t), screen_lerp_q12(from->y + from->h, to->y + to->h, t));
}

/*
 * Ease the current window's two quads from their present corners onto a pair
 * of destination rectangles. The interpolation parameter runs 0 -> 0x1000 in
 * decreasing steps, so the quads decelerate as they arrive; when the window is
 * growing (its quad area is smaller than the destination area) the sweep runs
 * a little past 0x1000 and then settles back down to it.
 */
static void screen_animate_window_quads_to_rects(menu_window_buffer_t* buffers, RECT* to_main, RECT* to_icon) {
    RECT from_main;
    RECT from_icon;
    s32 quad_area;
    s32 target_area;
    s32 step;
    s32 limit;
    s32 t;
    s32 frame;

    from_main.x = buffers->quads[0].x0;
    from_main.y = buffers->quads[0].y0;
    from_main.w = buffers->quads[0].x3 - from_main.x;
    from_main.h = buffers->quads[0].y3 - from_main.y;
    quad_area = from_main.w * from_main.h;
    target_area = to_main->w * to_main->h;

    if (to_icon->x != 0) {
        from_icon.x = buffers->quads[1].x0;
        from_icon.y = buffers->quads[1].y0;
        from_icon.w = buffers->quads[1].x3 - from_icon.x;
        from_icon.h = buffers->quads[1].y3 - from_icon.y;
    }

    /* Setting step here, after the icon block, keeps its live range short
     * enough for it to outrank buffers for a callee-saved register. */
    step = 0x400;
    limit = ONE;
    if (quad_area < target_area) {
        step = 0x380;
    }
    /* Initializing t between the two guards keeps jump.c from merging them
     * and supplies the second branch's delay-slot instruction. */
    t = 0;
    if (quad_area < target_area) {
        limit = 0x1060;
    }

    for (; t <= limit; t += step) {
        for (frame = 0; frame < 3 - g_script_event_speed; frame++) {
            thread_yield();
            screen_lerp_rect_to_poly_ft4(&buffers[g_menu_packet_buffer_index].quads[0], &from_main, to_main, t);
            if (to_icon->x != 0) {
                screen_lerp_rect_to_poly_ft4(&buffers[g_menu_packet_buffer_index].quads[1], &from_icon, to_icon, t);
            }
        }
        if (quad_area < target_area) {
            step -= 64;
            if (step < 64) {
                step = 64;
            }
        }
    }

    if (quad_area < target_area) {
        for (t = limit; t >= ONE; t -= 32) {
            for (frame = 0; frame < 3 - g_script_event_speed; frame++) {
                thread_yield();
                screen_lerp_rect_to_poly_ft4(&buffers[g_menu_packet_buffer_index].quads[0], &from_main, to_main, t);
                if (to_icon->x != 0) {
                    screen_lerp_rect_to_poly_ft4(&buffers[g_menu_packet_buffer_index].quads[1], &from_icon, to_icon, t);
                }
            }
        }
    }

    t = ONE;
    for (frame = 0; frame < 3 - g_script_event_speed; frame++) {
        thread_yield();
        screen_lerp_rect_to_poly_ft4(&buffers[g_menu_packet_buffer_index].quads[0], &from_main, to_main, t);
        if (to_icon->x != 0) {
            screen_lerp_rect_to_poly_ft4(&buffers[g_menu_packet_buffer_index].quads[1], &from_icon, to_icon, t);
        }
    }
}

/*
 * Twin of menu_animate_window_quad_crop without the two-pixel right
 * inset. Animate the current window's first quad growing out from the rectangle
 * center: each frame crops a sprite of the rectangle by the level's
 * percentage and copies the cropped bounds into the quad's corners.
 */
static void screen_animate_window_quad_crop(menu_window_buffer_t* buffers, RECT* rect, RECT* unused_to) {
    SPRT sprite;
    void* source; /* never assigned: a stale $s5 the callee never reads, as in menu_animate_window_quad_crop */
    s32 level;
    u8 u = buffers->quads[0].u0;
    u8 v = buffers->quads[0].v0;
    s32 unused[2]; /* two unreferenced locals account for the 0x50 frame */

    for (level = 0; level < 12; level += g_script_event_speed) {
        sprite.u0 = u;
        sprite.v0 = v;
        sprite.x0 = rect->x;
        sprite.y0 = rect->y;
        sprite.w = rect->w;
        sprite.h = rect->h;
        menu_zoom_cursor_frame(source, &sprite, level);
        setXY4(&buffers[g_menu_packet_buffer_index].quads[0], sprite.x0, sprite.y0, sprite.x0 + sprite.w, sprite.y0,
            sprite.x0, sprite.y0 + sprite.h, sprite.x0 + sprite.w, sprite.y0 + sprite.h);
        setUV4(&buffers[g_menu_packet_buffer_index].quads[0], sprite.u0, sprite.v0, sprite.u0 + sprite.w, sprite.v0,
            sprite.u0, sprite.v0 + sprite.h, sprite.u0 + sprite.w, sprite.v0 + sprite.h);
        thread_yield();
    }
}

/* Record `index` of the window buffer pair; the offset is the first addu
 * operand in the target, which plain `&win[index]` does not reproduce. */
#define WINDOW_AT(index) ((menu_window_buffer_t*)((index) * sizeof(menu_window_buffer_t) + (s32)win))

/* Advance the text cursor; a finished back-reference resumes at `saved`. */
#define NEXT()                                                                                                         \
    text++;                                                                                                            \
    if (repeat > 0) {                                                                                                  \
        repeat--;                                                                                                      \
        if (repeat == 0) {                                                                                             \
            text = saved;                                                                                              \
            saved = (u8*)-1;                                                                                           \
        }                                                                                                              \
    }

/* Message text box thread (task THREAD_TASK_MESSAGE_BOX).
 *
 * Draws the text entry named by thread parameter 2 glyph by glyph into a
 * window buffer pair, waiting for input at page breaks (0xF8/0xFE) and
 * restarting when parameter 2 changes. Parameter 1 selects the window style
 * (bits 4-6) and vertical position (bits 0-1). `no_value` is assigned inside
 * the loop so loop.c hoists it as an invariant: the resulting spilled
 * constant is what reproduces the target's 0xFFFF reloads and its reload
 * register rotation. */
void message_box_thread(void) {
    text_dialog_record_t rec;
    RECT from;
    RECT to;
    s32 column;
    s32 message;
    u8* base_entry;
    s16 x;
    s16 y;
    s16 cursor_x;
    s16 cursor_y;
    s16 right;
    s16 bottom;
    u16 delay;
    u16 no_wait;
    u16 script_variable_base;
    s32 width_override;
    u16 value_index;
    menu_window_buffer_t* win;
    menu_window_buffer_t* cursor_window;
    menu_window_buffer_t* arrow_window;
    menu_window_buffer_t* hidden_window;
    u8* text;
    u8* saved;
    s32 repeat;
    s32 code;
    s32 value;
    s32 i;
    s32 digits;
    u16 started;
    u16 param;
    u16 half_w;
    u16 half_h;
    s32* substitution;
    s32 no_value;

    thread_set_current_task_id(THREAD_TASK_MESSAGE_BOX);
    value_index = 0;
    script_variable_base = EVENT_SCRIPT_VAR_SELECTED_DIALOG_OPTION;
    g_screen_dialog_waiting = 0;
    param = thread_get_current_parameter_1();
    rec.box_type = param & 0x70;
    rec.dialog_type = param;
    rec.position = param & 3;
    rec.options = param & 0xC;
    text = text_find_entry(thread_get_current_parameter_2());
    base_entry = text_find_entry(thread_get_current_parameter_2() & 0xF800);

    /* Unlike a dialog, the box has a fixed size: 3 lines of 0x12 cells with a portrait, else 2 of 0x14. */
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        rec.mark = 0x12;
        rec.lines = 3;
    } else {
        rec.mark = 0x14;
        rec.lines = 2;
    }
    rec.height = rec.lines * 16 + 16;
    rec.width = rec.mark * 10 + 0x18;
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        rec.width = rec.mark * 10 + 0x40;
    }

    /* Center horizontally; positions 1, 2 and 3 are the top, bottom and middle. */
    rec.tail_offset = 0;
    half_w = rec.width >> 1;
    x = (0x100 - half_w) & ~1;
    half_h = rec.height >> 1;
    if (rec.position == 1) {
        y = 0xC;
    }
    if (rec.position == 2) {
        y = 0xE4 - rec.height;
    }
    if (rec.position == 3) {
        y = (0x78 - half_h) & ~1;
    }
    right = x + half_w - 6;
    bottom = y + half_h - 2;

    /* A leading portrait command picks the portrait; otherwise the last one shown is kept. */
    win = menu_alloc_window_buffer_pair();
    win->message_id = thread_get_current_parameter_2();
    if (text[0] == TEXT_SET_PORTRAIT) {
        win[0].portrait_code = text[1];
        win[1].portrait_code = text[1];
        script_set_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, text[1]);
    } else {
        win[0].portrait_code = script_get_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT);
        win[1].portrait_code = script_get_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT);
    }
    win[0].is_message_box = 1;
    win[1].is_message_box = 1;

    rec.text = thread_get_current_parameter_2();
    message = thread_get_current_parameter_2();
    rec.rect.w = rec.width;
    rec.rect.h = rec.height;
    gfx_alloc_texture_rect(&rec.rect, win->quads, (u32*)-1);
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        menu_build_and_upload_window_image(rec.width, rec.height, &rec.rect, rec.dialog_type, rec.tail_offset);
    } else {
        menu_build_and_upload_window_frame_image(rec.width, rec.height, &rec.rect, MENU_WINDOW_FRAME_UPLOAD);
    }

    /* The formation screen shifts its boxes by (-0x100, -0x78). */
    if (g_formation_screen_active != 0) {
        x -= 0x100;
        y -= 0x78;
        right -= 0x100;
        bottom -= 0x78;
    }

    /* Both quads start collapsed at the open point; the portrait sits at the left (flipped) in a top box. */
    setXY4(&win->quads[1], right, bottom, right, bottom, right, bottom, right, bottom);
    setXY4(&win->quads[0], right, bottom, right, bottom, right, bottom, right, bottom);
    from.x = x;
    from.y = y;
    from.w = rec.width;
    from.h = rec.height;
    if (rec.position == 1) {
        to.x = x + 8;
        to.y = y + 7;
        to.w = 0x1F;
        to.h = 0x30;
        screen_flip_poly_ft4_u(&win->quads[1], -1);
    } else {
        to.x = x + rec.width - 0x30;
        to.y = y + 8;
        to.w = 0x1F;
        to.h = 0x30;
    }

    /* The pair's two buffers alternate by frame, so the second starts as a copy of the first. */
    mem_copy_bytes_nonempty(&win[1], &win[0], sizeof(win[1]));
    win[0].active = 1;
    win[1].active = 1;
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        screen_animate_window_quads_to_rects(win, &from, &to);
    } else {
        screen_animate_window_quad_crop(win, &from, &to);
    }

    /* From here x and y address the window's image in VRAM, not the screen. */
    x = win->quads[0].u0 + 8;
    y = win->quads[0].v0 + 8;
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT && rec.position == 1) {
        x = win->quads[0].u0 + 0x30;
    }

    delay = 1;
    saved = (u8*)-1;
    repeat = 0;
    no_wait = 0;
    column = 0;
    rec.color = 0;
    rec.mark = -1;
    rec.line = 0;
    cursor_x = x;
    cursor_y = y;
    width_override = 0xFFFF;

    while (1) {
        no_value = 0xFFFF;
        code = *text;

        /* A new parameter 2 restarts the box on that message; 0xFFFF closes it. A message is
         * shown once its text section resolves to the same start as the first message's. */
        if (thread_get_current_parameter_2() != message) {
            if ((thread_get_current_parameter_2() & no_value) != no_value) {
                while (base_entry != text_find_entry(thread_get_current_parameter_2() & 0xF800)) {
                    thread_yield();
                }
            }
            message = thread_get_current_parameter_2();
            if ((message & no_value) == no_value) {
                g_screen_dialog_waiting = 0;
                break;
            }
            text = text_find_entry(message);
            if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
                menu_build_and_upload_window_image(rec.width, rec.height, &rec.rect, rec.dialog_type, rec.tail_offset);
            } else {
                menu_build_and_upload_window_frame_image(rec.width, rec.height, &rec.rect, MENU_WINDOW_FRAME_UPLOAD);
            }

            delay = 1;
            saved = (u8*)-1;
            cursor_x = x;
            cursor_y = y;
            no_wait = 0;
            column = 0;
            value_index = 0;
            rec.color = 0;
            rec.mark = -1;
            rec.line = 0;
            g_screen_dialog_waiting = 0;
            width_override = no_value;
            rec.text = message;
            repeat = 0;
            if (text[0] == TEXT_SET_PORTRAIT) {
                win[0].portrait_code = text[1];
                win[1].portrait_code = text[1];
                script_set_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, text[1]);
            } else {
                win[0].portrait_code = script_get_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT);
                win[1].portrait_code = script_get_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT);
            }
            thread_yield();
            continue;
        }

        if (code == TEXT_NEWLINE || code == TEXT_END_WAIT_FOR_CONFIRM || code == TEXT_END_AUTO_CLOSE
            || code == TEXT_PAGE_BREAK) {
            if (code == TEXT_END_AUTO_CLOSE) {
                break;
            }
            if (code == TEXT_END_WAIT_FOR_CONFIRM) {
                g_screen_dialog_waiting = 1;
                /* The end of a substituted string returns to the text that named it. */
                if (saved != (u8*)-1) {
                    text = saved;
                    saved = (u8*)-1;
                    continue;
                }
                while (1) {
                    if (column >= rec.lines) {
                        break;
                    }
                    column++;
                    rec.line++;
                }
                /* no_value (0xFFFF) steps the u16 line count back by one. */
                rec.line += no_value;
            } else {
                NEXT();
            }
            if (code != TEXT_PAGE_BREAK) {
                column++;
                rec.line++;
            }
            rec.first_line = rec.line - rec.lines;

            /* Pause when the box is full, at the end of the text and at a page break. Unlike a
             * dialog, the pause reads no buttons: it lasts until parameter 2 changes. */
            if (column >= rec.lines || code == TEXT_END_WAIT_FOR_CONFIRM || code == TEXT_PAGE_BREAK) {
                if (code != TEXT_PAGE_BREAK) {
                    column = 0;
                }
                if (no_wait == 0) {
                    for (i = 0;; i += g_script_event_speed) {
                        thread_yield();
                        if (thread_get_current_parameter_2() != message) {
                            break;
                        }
                        /* A page break bobs the cursor sprite after the text; a line break shows the arrow. */
                        if (code == TEXT_PAGE_BREAK) {
                            cursor_window = WINDOW_AT(g_menu_packet_buffer_index);
                            cursor_window->sprites[0].x0 = cursor_x
                                + (x + cursor_window->quads[0].x0 - cursor_window->quads[0].u0)
                                + (s16)(((i >> 4) & 3) - 8);
                            cursor_window->sprites[0].y0 = cursor_y
                                + (y + cursor_window->quads[0].y0 - cursor_window->quads[0].v0)
                                - (cursor_window->quads[0].v0 + 8);
                            cursor_window->sprites[1].x0 = cursor_window->sprites[0].x0 + 2;
                            cursor_window->sprites[1].y0 = cursor_window->sprites[0].y0 + 2;
                        } else {
                            if (code == TEXT_NEWLINE) {
                                WINDOW_AT(g_menu_packet_buffer_index)->sprites[2].u0 = (i & 0x30) - 0x58;
                                WINDOW_AT(g_menu_packet_buffer_index)->sprites[2].v0 = 0x10;
                                arrow_window = WINDOW_AT(g_menu_packet_buffer_index);
                                arrow_window->sprites[2].x0 = cursor_x + arrow_window->quads[0].x0;
                                arrow_window->sprites[2].y0
                                    = cursor_y + arrow_window->quads[0].y0 - arrow_window->quads[0].v0;
                            }

                            /* Scroll down to the newest lines. The else-break keeps the test on
                             * top, as in the target; a `while` is rotated to test at the bottom. */
                            for (;;) {
                                if (rec.line - rec.lines != rec.first_line) {
                                    WINDOW_AT(g_menu_packet_buffer_index)->sprites[2].x0 = 0;
                                    rec.first_line++;
                                    rec.last_line = rec.first_line + rec.lines;
                                    message_box_render_text_image(&rec);
                                } else {
                                    break;
                                }
                            }
                        }
                    }
                }

                /* Hide the cursor and arrow sprites in both buffers, one frame apart. */
                hidden_window = WINDOW_AT(g_menu_packet_buffer_index);
                hidden_window->sprites[0].x0 = 0x200;
                hidden_window->sprites[1].x0 = 0x200;
                hidden_window->sprites[2].x0 = 0x200;
                thread_yield();
                hidden_window = WINDOW_AT(g_menu_packet_buffer_index);
                hidden_window->sprites[0].x0 = 0x200;
                hidden_window->sprites[1].x0 = 0x200;
                hidden_window->sprites[2].x0 = 0x200;

                if (code == TEXT_PAGE_BREAK) {
                    continue;
                }
                if (thread_get_current_parameter_2() != message) {
                    continue;
                }

                /* The box is full: start a fresh page; cursor_y lands on its first line after the += below. */
                if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
                    menu_build_and_upload_window_image(
                        rec.width, rec.height, &rec.rect, rec.dialog_type, rec.tail_offset);
                } else {
                    menu_build_and_upload_window_frame_image(
                        rec.width, rec.height, &rec.rect, MENU_WINDOW_FRAME_UPLOAD);
                }
                cursor_y = y - 0x10;
                thread_yield();
            }

            no_wait = 0;
            cursor_x = x;
            cursor_y += 0x10;
        } else if (code == TEXT_CHOICE_MARK) {
            rec.mark = column;
            NEXT();
        } else if (code == TEXT_CHOICE_SELECT) {
            /* Message boxes offer no choices; the select code is skipped. */
            NEXT();
        } else if (code == TEXT_WAIT_FOR_RESUME) {
            /* No resume wait here: the next line break just skips its pause. */
            NEXT();
            no_wait = 1;
        } else if (code == TEXT_RELEASE_WAITING_THREADS) {
            thread_yield();
            NEXT();
        } else if (code == TEXT_SET_PORTRAIT) {
            NEXT();
            win[0].portrait_code = *text;
            win[1].portrait_code = *text;
            script_set_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, *text);
            NEXT();
        } else if (code == TEXT_SET_VARIABLE_BASE) {
            NEXT();
            script_variable_base = *text;
            NEXT();
        } else if (code == TEXT_STORE_VARIABLE) {
            NEXT();
            i = script_get_variable(EVENT_SCRIPT_VAR_TEXT_STORE_INDEX);
            if (i != 0) {
                script_set_variable(script_variable_base + i * 10, *text);
                script_set_variable(EVENT_SCRIPT_VAR_TEXT_STORE_INDEX, 0);
            } else {
                script_set_variable(script_variable_base, *text);
            }
            NEXT();
        } else if (code == TEXT_SET_GLYPH_DELAY) {
            NEXT();
            delay = *text;
            NEXT();
        } else if (code == TEXT_SET_PALETTE) {
            NEXT();
            rec.color = *text;
            NEXT();
        } else {
            /* A format code draws a whole string, then returns to `saved`. */
            substitution = &g_text_substitution_values[value_index];
            value = text_resolve_format_string_id(code, *substitution);
            if (value != -1) {
                saved = text + 1;
                value_index++;
                text = text_find_entry(value);
                continue;
            }

            if (code == TEXT_SET_NUMBER_WIDTH) {
                NEXT();
                width_override = *text;
                NEXT();
            } else if (code == TEXT_PRINT_NEXT_VALUE || code == TEXT_PRINT_INDEXED_VALUE) {
                if (code == TEXT_PRINT_NEXT_VALUE) {
                    value = *substitution;
                    value_index++;
                } else {
                    NEXT();
                    value = g_text_substitution_values[*text];
                }

                /* Print the number in `digits` places, padding leading zeros with spaces. */
                digits = text_count_decimal_digits(value);
                started = 0;
                if (width_override != no_value) {
                    digits = width_override;
                    width_override = no_value;
                }
                for (i = digits - 1; i >= 0; i--) {
                    code = value / g_message_box_digit_divisors[i];
                    if (value < g_message_box_digit_divisors[i] && i != 0 && started == 0) {
                        code = TEXT_SPACE;
                    } else {
                        started = 1;
                        value -= code * g_message_box_digit_divisors[i];
                    }
                    g_message_box_glyph.glyph = code;
                    g_message_box_glyph.x = cursor_x;
                    g_message_box_glyph.y = cursor_y;
                    g_message_box_glyph.mode = rec.dialog_type & DIALOG_TYPE_BOX_TYPE_MASK;
                    g_message_box_glyph.palette = rec.color;
                    message_box_draw_glyph(cursor_x, cursor_y, delay);
                    if (code == TEXT_SPACE) {
                        cursor_x += 4;
                    } else {
                        cursor_x += g_text_glyph_widths[code];
                    }
                }
                NEXT();
            } else if ((code & 0xF0) == TEXT_BACKREFERENCE_PREFIX_FIRST && (code & 0xF) < 4) {
                /* Compressed text: replay `repeat` bytes from earlier in the entry, then resume at `saved`. */
                saved = text + 3;
                value = (text[0] & 3) * 8;
                repeat = text[1] >> 5;
                repeat += value;
                repeat += 4;
                value = text[1] & 0xF;
                value *= 0xFE;
                value = value + text[2];
                text -= value;
            } else {
                if (code < TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
                    code = *text;
                } else if ((code & 0xF0) == TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
                    /* A two-byte glyph: the prefix's low nibble selects a page of glyphs. */
                    code = (*text & 0xF) * TEXT_GLYPHS_PER_PREFIX;
                    NEXT();
                    code = code + *text;
                }

                g_message_box_glyph.glyph = code;
                g_message_box_glyph.mode = rec.box_type;
                g_message_box_glyph.palette = rec.color;
                g_message_box_glyph.x = cursor_x;
                g_message_box_glyph.y = cursor_y;
                message_box_draw_glyph(cursor_x, cursor_y, delay);
                if (code == TEXT_SPACE) {
                    cursor_x += 4;
                } else {
                    cursor_x += g_text_glyph_widths[code];
                }
                NEXT();
            }
        }
    }

    /* Close: a portrait box shrinks back to its open point; then release the window, texture and glyph slot. */
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        from.w = 0;
        from.h = 0;
        from.x = right;
        from.y = bottom;
        mem_copy_bytes_nonempty(&to, &from, sizeof(to));
        screen_animate_window_quads_to_rects(win, &from, &to);
    }
    win->active = 0;
    menu_release_window_buffer_pair(win);
    gfx_free_texture_rect(&rec.rect);
    message_box_release_slot();
    thread_yield();
    thread_exit_current();
}
