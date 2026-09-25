#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Two draw modes and three sprites, double-buffered by the thread below. */
typedef struct world_menu_text_window_record {
    DR_MODE mode_a;
    DR_MODE mode_b;
    SPRT sprite_a;
    SPRT sprite_b;
    SPRT sprite_c;
} world_menu_text_window_record_t;

/* Signed window view of world_menu_entry_t: the target clamps the x/y origin
 * at 0x08/0x0a as signed halfwords (world_menu_entry_t types 0x08 as u16) and
 * reads a pointer to two text substitution words at 0x34. */
typedef struct world_menu_text_window_entry {
    s16 image_x;       /* 0x00 */
    s16 image_y;       /* 0x02 */
    s16 width;         /* 0x04 */
    s16 height;        /* 0x06 */
    s16 x;             /* 0x08 */
    s16 y;             /* 0x0a */
    s16 screen_width;  /* 0x0c */
    s16 screen_height; /* 0x0e */
    u8 unknown_10[4];
    s16 image_width;  /* 0x14 */
    s16 image_height; /* 0x16 */
    u8 unknown_18[4];
    s16 text_id; /* 0x1c */
    u8 unknown_1e[0x34 - 0x1e];
    s32* text_values; /* 0x34 */
    u8 unknown_38[0x3c - 0x38];
} world_menu_text_window_entry_t;

/* Main menu text window thread:
 * sizes the entry's window to its text, clamps it on screen, redraws the text
 * image every 32 frames and, while the main menu (task 0x21) scrolls, slides
 * the window in 8-pixel steps toward the scroll-dependent target x. */
void world_main_menu_text_window_thread(void) {
    RECT rect;
    world_menu_text_window_record_t records[2];
    s16 width;
    s16 height;
    world_menu_text_window_entry_t* entry;
    world_menu_text_window_record_t* record;
    void* buffer;
    s32 i;
    s32 phase;
    s32 pad;
    s16 target;
    s32 x;

    entry = world_thread_get_current_parameter_1();
    record = records;
    world_gfx_set_draw_mode_from_rect(&record->mode_a, (u16*)entry);
    world_gfx_set_image_draw_mode(&record->mode_b, 0);
    /* The target also passes a2 = 0 to this two-argument callee. */
    ((void (*)(world_menu_text_window_entry_t*, SPRT*, s32))world_menu_build_layout_sprites)(
        entry, &record->sprite_b, 0);
    world_script_copy_bytes(g_world_text_substitution_values_backup, g_world_text_substitution_values, 0x80);
    g_world_text_substitution_values[0] = entry->text_values[0];
    g_world_text_substitution_values[1] = entry->text_values[1];
    world_text_measure(&width, &height, world_text_find_entry(entry->text_id));
    world_script_copy_bytes(g_world_text_substitution_values, g_world_text_substitution_values_backup, 0x80);
    /* 10-pixel columns plus a 0x18 border, rounded up past the next multiple of four. */
    x = width * 10;
    pad = ((x & 2) == 0) * 4;
    x += 0x18;
    width = x + (x & 3) + pad;
    height = height * 16 + 16;
    entry->width = width;
    entry->screen_width = width;
    entry->image_width = width;
    entry->height = height;
    entry->screen_height = height;
    entry->image_height = height;
    if (entry->x + width > 0x80) {
        entry->x = 0x78 - width;
    } else if (entry->x < -0x80) {
        entry->x = -0x78;
    }
    if (entry->y + height > 0x78) {
        entry->y = 0x68 - height;
    } else if (entry->y < -0x78) {
        entry->y = -0x68;
    }
    /* The callee types this RECT, record and SPRT as raw u16 and POLY_FT4 pointers. */
    ((void (*)(RECT*, world_menu_text_window_entry_t*, SPRT*))world_menu_init_quad_from_record)(
        &rect, entry, &record->sprite_a);
    SetSemiTrans(&record->sprite_a, 1);
    SetShadeTex(&record->sprite_a, 0);
    SetSemiTrans(&record->sprite_b, 1);
    SetShadeTex(&record->sprite_b, 0);
    world_script_copy_bytes(&records[1], &records[0], sizeof(world_menu_text_window_record_t));
    target = 0;
    for (i = 0;; i++) {
        record = &records[i & 1];
        phase = i & 0x1F;
        if (phase == 0) {
            buffer = world_menu_build_and_upload_window_frame_image(entry->width, entry->height, &rect, 1);
            g_world_menu_text_state.stride = entry->width;
            world_menu_set_text_origin(10, 8);
            g_world_menu_text_color = 0;
            world_script_copy_bytes(g_world_text_substitution_values_backup, g_world_text_substitution_values, 0x80);
            g_world_text_substitution_values[0] = entry->text_values[0];
            g_world_text_substitution_values[1] = entry->text_values[1];
            world_menu_display_text_entry(entry->text_id, buffer, &g_world_menu_text_state.origin_x);
            world_script_copy_bytes(g_world_text_substitution_values, g_world_text_substitution_values_backup, 0x80);
            LoadImage(&rect, buffer);
        }
        world_thread_yield();
        if (phase == 0) {
            world_menu_free_memory(buffer);
        }
        if (world_thread_get_current_parameter_3() != 0) {
            break;
        }
        if (g_world_menu_use_alternate_main_palette != 0) {
            record->sprite_a.clut = 0x7D3C;
            record->sprite_b.clut = 0x7C7C;
        } else {
            record->sprite_a.clut = 0x7C3C;
            record->sprite_b.clut = 0x7CBC;
        }
        if (g_world_threads[g_world_thread_current_id].task_id == 0x21) {
            if (g_world_menu_main_scroll_threshold > 0x28 && target == 0) {
                if (g_world_menu_main_scroll_direction > 0) {
                    if (entry->x >= -0x3F) {
                        target = -0x78;
                    }
                } else if (entry->x < -0x3F) {
                    target = 0x78 - width;
                }
            }
            if (target != 0) {
                if (entry->x > target) {
                    entry->x -= 8;
                    if (entry->x <= target) {
                        entry->x = target;
                        target = 0;
                    }
                } else if (entry->x < target) {
                    entry->x += 8;
                    if (entry->x >= target) {
                        entry->x = target;
                        target = 0;
                    }
                }
            }
            record->sprite_a.x0 = entry->x;
        }
        record->sprite_b.r0 = g_world_menu_color_red;
        record->sprite_b.g0 = g_world_menu_color_green;
        record->sprite_b.b0 = g_world_menu_color_blue;
        world_gfx_draw_or_append_gpu_primitive(&record->sprite_b);
        world_gfx_draw_or_append_gpu_primitive(&record->mode_b);
        record->sprite_a.r0 = g_world_menu_color_red;
        record->sprite_a.g0 = g_world_menu_color_green;
        record->sprite_a.b0 = g_world_menu_color_blue;
        world_gfx_draw_or_append_gpu_primitive(&record->sprite_a);
        world_gfx_draw_or_append_gpu_primitive(&record->mode_a);
    }
    world_thread_yield();
    world_thread_exit_current();
}
