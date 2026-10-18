/* The Formation status banner: portraits, identity text, gauges and animated status panels. */

#include "fft/formation.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/portrait.h"
#include "fft/screen.h"
#include "fft/script.h"
#include "fft/status_screen.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* banner drawing data */
extern CVECTOR g_formation_panel_bar_colors[];
extern image_layout_t g_formation_summary_label_sprite_params[];
extern image_layout_t g_formation_summary_value_sprite_params[];
extern RECT g_formation_status_panel_origin;
extern image_layout_t g_formation_status_sprite_params[];
extern image_layout_t g_formation_status_portrait_params;
extern menu_number_entry_t g_formation_selected_unit_number_entries[];
extern menu_number_entry_t g_formation_comparison_unit_number_entries[];

/* Selected- and comparison-unit panel storage. Packet and portrait palette
 * arrays are indexed by the active page. */
extern menu_status_panel_editor_packet_t g_formation_selected_unit_summary_packets[2];
extern menu_status_panel_packet_t g_formation_selected_unit_status_packets[2];
extern u8 g_formation_selected_unit_stat_text_image[];
extern u8 g_formation_selected_unit_name_text_image[];
extern menu_unit_number_images_t g_formation_selected_unit_number_text_images;
extern palette_t g_formation_selected_unit_portrait_palettes[2];
extern menu_status_panel_editor_packet_t g_formation_comparison_unit_summary_packets[2];
extern menu_status_panel_packet_t g_formation_comparison_unit_status_packets[2];
extern u8 g_formation_comparison_unit_stat_text_image[];
extern u8 g_formation_comparison_unit_name_text_image[];
extern menu_unit_number_images_t g_formation_comparison_unit_number_text_images;
extern palette_t g_formation_comparison_unit_portrait_palettes[2];

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
 *  - dims the banner while a THREAD_TASK_STATUS_PANEL thread runs under an input
 *    lock, or the display record asks for it;
 *  - runs the slide-down or slide-up table once when the display flags ask
 *    for it, leaving only MENU_STATUS_PANEL_HIDDEN set at the end;
 *  - cross-fades the portrait when the shown unit changes
 *    (portrait_build_transition);
 *  - redraws the number and name images on the first frame and whenever
 *    thread->redraw_request is set. While the preview-stats window is open the first
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
