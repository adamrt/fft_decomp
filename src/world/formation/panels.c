/* Formation status panels list a unit's equipment or abilities in paired windows. The two threads share the panel
 * layout while keeping separate draw-area packets. */

#include "fft/formation.h"
#include "fft/formation_ability.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/item.h"
#include "fft/item_menu.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/screen.h"
#include "fft/script.h"
#include "fft/status_screen.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit_panel.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* panel drawing data */
static unit_panel_buffer_t g_formation_stat_preview_panel_frames[2];
extern RECT g_formation_ability_panel_origin;

static RECT g_formation_equipment_panel_draw_area = { 132, 126, 141, 108 };
static RECT g_formation_ability_panel_draw_area = { 247, 126, 133, 108 };

/*
 * Equipment panel thread (task THREAD_TASK_STATUS_PANEL), the equipment twin
 * of unit_panel_ability_thread (same 0x3d8-byte frame pair, thread record
 * and callee sequence). It lists the five equipment names of an item
 * stat-detail record and draws their item icons: in the equip/ability panel
 * slot g_formation_selected_unit_stat_detail, in any other slot (the stat preview)
 * g_item_preview_stat_detail. The panel layout (0-3) comes from the record's
 * Two Hands and generic-monster flags; the text image is rebuilt on the first
 * frame or on request. Runs until parameter 3 is set.
 *
 * The first text loop re-reads equipment[] instead of caching it: the target
 * reloads the slot as a byte after the color store, and the color field goes
 * through a signed temporary because the target shifts it with sra. The empty
 * icon store's odd indexing also matches the target's address arithmetic.
 */
void unit_panel_equipment_thread(void) {
    u8* text;
    s32 u;
    unit_panel_buffer_t* base;
    menu_status_panel_slot_state_t* state;
    RECT* upload;
    unit_panel_thread_t* thread;
    unit_panel_buffer_t* screen;
    s32 frame;
    s32 mode;
    s32 color_bits;
    s16 id;
    s32 i;

    thread_current_task_set(THREAD_TASK_STATUS_PANEL);
    thread = (unit_panel_thread_t*)g_threads[g_thread_current_id].function_parameter_1;
    g_menu_input_controller = script_get_controller_input_pointer(0);
    if (g_thread_current_id == SCREEN_THREAD_SLOT_EQUIP_ABILITY_PANEL) {
        text = g_unit_panel_text_image_a;
        screen = g_unit_panel_equipment_frames;
        state = &g_formation_selected_unit_stat_detail.state;
        base = g_unit_panel_equipment_frames;
        upload = &g_unit_panel_text_upload_rect_a;
        u = 0;
    } else {
        text = g_unit_panel_text_image_b;
        screen = g_formation_stat_preview_panel_frames;
        state = &g_item_preview_stat_detail.state;
        upload = &g_unit_panel_text_upload_rect_b;
        base = g_formation_stat_preview_panel_frames;
        u = 0x50;
    }

    status_screen_init_numbers_frame(&g_unit_panel_equipment_origins[0], &screen->numeric_frame);
    unit_panel_init_primitives(&screen->frame);
    screen_panel_init_scaled_draw_area_packets(&screen->portrait);
    screen->frame.tiles[0].x0 += g_unit_panel_equipment_origins[0].x;
    screen->frame.tiles[0].y0 += g_unit_panel_equipment_origins[0].y;
    for (i = 0; i < 4; i++) {
        screen->frame.lines[i].x0 += g_unit_panel_equipment_origins[0].x;
        screen->frame.lines[i].y0 += g_unit_panel_equipment_origins[0].y;
        screen->frame.lines[i].x1 += g_unit_panel_equipment_origins[0].x;
        screen->frame.lines[i].y1 += g_unit_panel_equipment_origins[0].y;
    }

    menu_init_sprite_array(&screen->frame.sprites[0], 12, 0x7d7c);
    menu_init_sprite_array(&screen->frame.sprites[12], 7, MENU_CLUT_NORMAL);
    screen->frame.sprites[0].clut = 0x7cbc;
    screen->frame.sprites[1].clut = 0x7cbc;
    mem_copy_bytes_nonempty(&screen[1], screen, sizeof(unit_panel_buffer_t));

    frame = 0;
    for (;;) {
        screen = &base[frame & 1];

        mode = state->two_hands != 0 ? 2 : 0;
        if (state->generic_monster != 0) {
            mode++;
        }
        for (i = 0; i < 19; i++) {
            if (mode == 0) {
                gfx_set_image_primitive_layout(&screen->frame.sprites[i], &g_unit_panel_texture_origin,
                    &g_unit_panel_equipment_origins[0], &g_unit_panel_label_layouts_mode0[i]);
            }
            if (mode == 1) {
                gfx_set_image_primitive_layout(&screen->frame.sprites[i], &g_unit_panel_texture_origin,
                    &g_unit_panel_equipment_origins[0], &g_unit_panel_label_layouts_mode1[i]);
            }
            if (mode == 2) {
                gfx_set_image_primitive_layout(&screen->frame.sprites[i], &g_unit_panel_texture_origin,
                    &g_unit_panel_equipment_origins[0], &g_unit_panel_label_layouts_mode2[i]);
            }
            if (mode == 3) {
                gfx_set_image_primitive_layout(&screen->frame.sprites[i], &g_unit_panel_texture_origin,
                    &g_unit_panel_equipment_origins[0], &g_unit_panel_label_layouts_mode3[i]);
            }
        }

        if (frame == 0 || thread->redraw_request == 1) {
            menu_clear_render_buffer(text, 0xc80);
            g_menu_window_text_pen.stride = 0x50;
            menu_set_text_origin(0, 0);
            for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
                if ((state->equipment[i] & ITEM_ID_MASK) != ITEM_ID_NONE) {
                    color_bits = state->equipment[i] & FORMATION_PALETTE_MASK;
                    g_menu_text_palette_offset = color_bits >> 12;
                    menu_display_text_entry(
                        (state->equipment[i] & ITEM_ID_MASK) | TEXT_ID_ITEM_NAME_BASE, text, &g_menu_window_text_pen);
                }
                g_menu_window_text_pen.rect.y += 0x10;
            }
            LoadImage(upload, (u32*)text);
            thread->redraw_request = 0;
        }

        screen->frame.sprites[12].u0 = u;
        screen->frame.sprites[13].u0 = u;
        menu_init_sprite_array(&screen->frame.sprites[19], MENU_EQUIPMENT_SLOT_COUNT, 0x7cfc);
        for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
            gfx_set_image_primitive_layout(&screen->frame.sprites[19 + i], &g_unit_panel_item_icon_texture_origin,
                &g_unit_panel_equipment_origins[0], &g_unit_panel_item_icon_layouts[i]);
            id = state->equipment[i];
            if ((id & ITEM_ID_MASK) != ITEM_ID_NONE) {
                gfx_set_item_icon_sprite(&screen->frame.sprites[19 + i], id);
            } else {
                (&screen->frame.sprites[i])[19].x0 = 0;
            }
        }

        /* Repeats the two stores above; nothing in between touches sprites 12 and 13. */
        screen->frame.sprites[12].u0 = u;
        screen->frame.sprites[13].u0 = u;

        screen->draw_offsets[0].position.x = thread->position.x - 0x80;
        g_gfx_draw_offset_y = g_menu_framebuffer_y != 0xf0 ? 0xf0 : 0;
        screen->draw_offsets[0].position.y = thread->position.y + g_gfx_draw_offset_y;
        SetDrawOffset(&screen->draw_offsets[0].packet, (u16*)&screen->draw_offsets[0].position);
        screen->draw_offsets[1].position.x = -0x80;
        screen->draw_offsets[1].position.y = g_gfx_draw_offset_y;
        SetDrawOffset(&screen->draw_offsets[1].packet, (u16*)&screen->draw_offsets[1].position);
        menu_build_scaled_draw_area_packets(
            &screen->portrait, &g_formation_equipment_panel_draw_area, frame, g_menu_framebuffer_y, &thread->position);
        unit_panel_set_colors(&screen->frame, thread);
        unit_panel_select_palette(&screen->numeric_frame, thread);

        gfx_draw_or_append_gpu_primitive(&screen->portrait.areas[1]);
        gfx_draw_or_append_gpu_primitive(&screen->draw_offsets[1]);
        gfx_draw_or_append_gpu_primitive(&screen->frame.sprites[0]);
        for (i = 2; i < 7; i++) {
            gfx_draw_or_append_gpu_primitive(&screen->frame.sprites[i]);
        }
        for (i = 14; i < 19; i++) {
            gfx_draw_or_append_gpu_primitive(&screen->frame.sprites[i]);
        }
        gfx_draw_or_append_gpu_primitive(&screen->frame.frame_draw_mode);
        gfx_draw_or_append_gpu_primitive(&screen->frame.sprites[12]);
        gfx_draw_or_append_gpu_primitive(&screen->frame.text_draw_mode);
        for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
            gfx_draw_or_append_gpu_primitive(&screen->frame.sprites[19 + i]);
        }
        gfx_draw_or_append_gpu_primitive(&screen->frame.image_draw_mode);
        gfx_draw_or_append_gpu_primitive(&screen->frame.tiles[0]);
        for (i = 0; i < 4; i++) {
            gfx_draw_or_append_gpu_primitive(&screen->frame.lines[i]);
        }
        status_screen_submit_numbers_frame(&screen->numeric_frame);
        gfx_draw_or_append_gpu_primitive(&screen->draw_offsets[0]);
        gfx_draw_or_append_gpu_primitive(&screen->portrait);

        thread_yield();
        if (thread_current_parameter_3_get() != 0) {
            break;
        }
        frame++;
    }

    thread_yield();
    thread_current_exit();
}

/*
 * Ability panel thread (task THREAD_TASK_STATUS_PANEL): a two-column status
 * panel that lists five ability labels of an item stat-detail record. The
 * equip/ability panel slot draws g_formation_selected_unit_stat_detail into the
 * first frame pair, other slots draw g_item_preview_stat_detail into the
 * second. A trimmed relative of unit_panel_equipment_ability_thread (same 0x3d8-byte
 * frame layout and callee sequence). Runs until parameter 3 is set.
 */
void unit_panel_ability_thread(void) {
    point32_t offset;
    /* Never referenced: the target frame has these 16 bytes between the
     * offset pair and the spill slots of u/base/upload. */
    s32 unused[4];
    s32 u;
    unit_panel_buffer_t* base;
    RECT* upload;
    unit_panel_thread_t* thread;
    u8* text;
    unit_panel_buffer_t* screen;
    menu_status_panel_slot_state_t* state;
    s32 frame;
    s32 i;
    s32 id;

    thread_current_task_set(THREAD_TASK_STATUS_PANEL);
    thread = (unit_panel_thread_t*)g_threads[g_thread_current_id].function_parameter_1;
    g_menu_input_controller = script_get_controller_input_pointer(0);
    if (g_thread_current_id == SCREEN_THREAD_SLOT_EQUIP_ABILITY_PANEL) {
        text = g_unit_panel_text_image_a;
        screen = g_unit_panel_equipment_frames;
        state = &g_formation_selected_unit_stat_detail.state;
        base = g_unit_panel_equipment_frames;
        upload = &g_unit_panel_text_upload_rect_a;
        u = 0;
    } else {
        text = g_unit_panel_text_image_b;
        screen = g_formation_stat_preview_panel_frames;
        state = &g_item_preview_stat_detail.state;
        upload = &g_unit_panel_text_upload_rect_b;
        base = g_formation_stat_preview_panel_frames;
        u = 0x50;
    }

    status_screen_init_numbers_frame(&g_formation_ability_panel_origin, &screen->numeric_frame);
    unit_panel_init_primitives(&screen->frame);
    screen_panel_init_scaled_draw_area_packets(&screen->portrait);
    screen->frame.tiles[1].x0 += g_unit_panel_equipment_origins[0].x;
    screen->frame.tiles[1].y0 += g_unit_panel_equipment_origins[0].y;
    for (i = 4; i < 8; i++) {
        screen->frame.lines[i].x0 += g_unit_panel_equipment_origins[0].x;
        screen->frame.lines[i].y0 += g_unit_panel_equipment_origins[0].y;
        screen->frame.lines[i].x1 += g_unit_panel_equipment_origins[0].x;
        screen->frame.lines[i].y1 += g_unit_panel_equipment_origins[0].y;
    }

    menu_init_sprite_array(&screen->frame.sprites[0], 12, 0x7d7c);
    menu_init_sprite_array(&screen->frame.sprites[12], 7, MENU_CLUT_NORMAL);
    screen->frame.sprites[0].clut = 0x7cbc;
    screen->frame.sprites[1].clut = 0x7cbc;
    mem_copy_bytes_nonempty(&screen[1], screen, sizeof(unit_panel_buffer_t));

    frame = 0;
    for (;;) {
        screen = &base[frame & 1];

        if (state->generic_monster == 0) {
            for (i = 0; i < 19; i++) {
                gfx_set_image_primitive_layout(&screen->frame.sprites[i], &g_unit_panel_texture_origin,
                    &g_unit_panel_equipment_origins[0], &g_unit_panel_label_layouts_mode0[i]);
            }
        } else {
            for (i = 0; i < 19; i++) {
                gfx_set_image_primitive_layout(&screen->frame.sprites[i], &g_unit_panel_texture_origin,
                    &g_unit_panel_equipment_origins[0], &g_unit_panel_label_layouts_mode1[i]);
            }
        }

        /* A generic monster's empty ability rows move their labels 0x200 pixels left, off screen. */
        if (state->generic_monster != 0) {
            for (i = 0; i < 5; i++) {
                u16 ability = state->ability_ids[i];
                if (ability == MENU_STATUS_PANEL_LABEL_NONE || ability == 0) {
                    screen->frame.sprites[7 + i].x0 -= 0x200;
                }
            }
        }

        /* No i = 0 here: the target's reset before this test is reorg
         * hoisting the loop's own initialization into a delay slot, and an
         * explicit one lengthens i's live range enough to lose it $s1. */
        if (frame == 0 || thread->redraw_request == 1) {
            menu_clear_render_buffer(text, 0xc80);
            g_menu_window_text_pen.stride = 0x50;
            menu_set_text_origin(0, 0);
            /* Rows 0-1 name skillsets (abilities for a generic monster); rows 2-4 name abilities. */
            for (i = 0; i < 2; i++) {
                id = state->ability_ids[i] + TEXT_ID_SKILLSET_NAME_SOURCE_BASE;
                if (state->generic_monster != 0) {
                    id = state->ability_ids[i] + TEXT_ID_ABILITY_NAME_BASE;
                }
                if ((u16)state->ability_ids[i] == MENU_STATUS_PANEL_LABEL_NONE) {
                    g_menu_window_text_pen.rect.y += 0x10;
                } else {
                    menu_display_text_entry(id, text, &g_menu_window_text_pen);
                    g_menu_window_text_pen.rect.y += 0x10;
                }
            }
            for (; i < 5; i++) {
                if ((u16)state->ability_ids[i] != MENU_STATUS_PANEL_LABEL_NONE) {
                    menu_display_text_entry(
                        state->ability_ids[i] + TEXT_ID_ABILITY_NAME_BASE, text, &g_menu_window_text_pen);
                }
                g_menu_window_text_pen.rect.y += 0x10;
            }
            LoadImage(upload, (u32*)text);
            thread->redraw_request = 0;
        }

        screen->frame.sprites[12].u0 = u;
        screen->frame.sprites[13].u0 = u;

        screen->draw_offsets[0].position.x = thread->position.x - 0xf3;
        g_gfx_draw_offset_y = g_menu_framebuffer_y != 0xf0 ? 0xf0 : 0;
        screen->draw_offsets[0].position.y = thread->position.y + g_gfx_draw_offset_y;
        SetDrawOffset(&screen->draw_offsets[0].packet, (u16*)&screen->draw_offsets[0].position);
        screen->draw_offsets[1].position.x = -0x80;
        screen->draw_offsets[1].position.y = g_gfx_draw_offset_y;
        SetDrawOffset(&screen->draw_offsets[1].packet, (u16*)&screen->draw_offsets[1].position);
        offset.x = thread->position.x - 0x73;
        offset.y = thread->position.y;
        menu_build_scaled_draw_area_packets(
            &screen->portrait, &g_formation_ability_panel_draw_area, frame, g_menu_framebuffer_y, &offset);
        unit_panel_set_colors(&screen->frame, thread);
        unit_panel_select_palette(&screen->numeric_frame, thread);

        gfx_draw_or_append_gpu_primitive(&screen->portrait.areas[1]);
        gfx_draw_or_append_gpu_primitive(&screen->draw_offsets[1]);
        gfx_draw_or_append_gpu_primitive(&screen->frame.sprites[1]);
        for (i = 7; i < 12; i++) {
            gfx_draw_or_append_gpu_primitive(&screen->frame.sprites[i]);
        }
        gfx_draw_or_append_gpu_primitive(&screen->frame.frame_draw_mode);
        gfx_draw_or_append_gpu_primitive(&screen->frame.sprites[13]);
        gfx_draw_or_append_gpu_primitive(&screen->frame.text_draw_mode);
        gfx_draw_or_append_gpu_primitive(&screen->frame.tiles[1]);
        for (i = 4; i < 8; i++) {
            gfx_draw_or_append_gpu_primitive(&screen->frame.lines[i]);
        }
        status_screen_submit_numbers_frame(&screen->numeric_frame);
        gfx_draw_or_append_gpu_primitive(&screen->draw_offsets[0]);
        gfx_draw_or_append_gpu_primitive(&screen->portrait);

        thread_yield();
        if (thread_current_parameter_3_get() != 0) {
            break;
        }
        frame++;
    }

    thread_yield();
    thread_current_exit();
}
