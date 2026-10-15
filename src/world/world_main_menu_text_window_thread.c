#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* The text sprite and the two frame sprites, double-buffered by the thread below. */
typedef struct world_menu_text_window_record {
    DR_MODE text_draw_mode;
    DR_MODE frame_draw_mode;
    SPRT text_sprite;
    SPRT frame_sprites[2];
} world_menu_text_window_record_t;
typedef char world_menu_text_window_record_size_must_be_84[sizeof(world_menu_text_window_record_t) == 84 ? 1 : -1];

/* Main menu text window thread:
 * sizes the entry's window to its text, clamps it on screen, redraws the text
 * image every 32 frames and, while the main menu (task 0x21) scrolls, slides
 * the window in 8-pixel steps toward the scroll-dependent target x. */
void world_main_menu_text_window_thread(void) {
    RECT rect;
    world_menu_text_window_record_t records[2];
    s16 width;
    s16 height;
    menu_entry_t* entry;
    s32* text_values;
    world_menu_text_window_record_t* record;
    u8* buffer;
    s32 i;
    s32 phase;
    s32 pad;
    s16 target;
    s32 x;

    entry = (menu_entry_t*)thread_get_current_parameter_1_variant();
    record = records;
    gfx_set_draw_mode_from_origin(&record->text_draw_mode, &entry->vram_origin);
    gfx_set_draw_mode_for_texture_page(&record->frame_draw_mode, MENU_TEXTURE_PAGE_FRAME);
    world_menu_build_layout_sprites(entry, record->frame_sprites, 0);
    mem_copy_bytes_nonempty(g_world_text_substitution_values_backup, g_text_substitution_values,
        sizeof(g_world_text_substitution_values_backup));
    text_values = entry->value;
    g_text_substitution_values[0] = text_values[0];
    text_values = entry->value;
    g_text_substitution_values[1] = text_values[1];
    text_measure(&width, &height, text_find_entry(entry->text_id));
    mem_copy_bytes_nonempty(
        g_text_substitution_values, g_world_text_substitution_values_backup, sizeof(g_text_substitution_values));
    /* 10-pixel columns plus a 0x18 border, rounded up past the next multiple of four. */
    x = width * 10;
    pad = ((x & 2) == 0) * 4;
    x += 0x18;
    width = x + (x & 3) + pad;
    height = height * 16 + 16;
    entry->inner_width = width;
    entry->window.w = width;
    entry->text_image.width = width;
    entry->inner_height = height;
    entry->window.h = height;
    entry->text_image.height = height;
    if (entry->window.x + width > 0x80) {
        entry->window.x = 0x78 - width;
    } else if (entry->window.x < -0x80) {
        entry->window.x = -0x78;
    }
    if (entry->window.y + height > 0x78) {
        entry->window.y = 0x68 - height;
    } else if (entry->window.y < -0x78) {
        entry->window.y = -0x68;
    }
    world_menu_init_quad_from_record(&rect, entry, &record->text_sprite);
    SetSemiTrans(&record->text_sprite, 1);
    SetShadeTex(&record->text_sprite, 0);
    SetSemiTrans(&record->frame_sprites[0], 1);
    SetShadeTex(&record->frame_sprites[0], 0);
    mem_copy_bytes_nonempty(&records[1], &records[0], sizeof(world_menu_text_window_record_t));
    target = 0;
    for (i = 0;; i++) {
        record = &records[i & 1];
        phase = i & 0x1F;
        if (phase == 0) {
            buffer = menu_build_and_upload_window_frame_image_variant(
                entry->inner_width, entry->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
            g_menu_text_state.pen.stride = entry->inner_width;
            menu_set_text_origin(10, 8);
            g_menu_text_palette_offset = 0;
            mem_copy_bytes_nonempty(g_world_text_substitution_values_backup, g_text_substitution_values,
                sizeof(g_world_text_substitution_values_backup));
            text_values = entry->value;
            g_text_substitution_values[0] = text_values[0];
            text_values = entry->value;
            g_text_substitution_values[1] = text_values[1];
            menu_display_text_entry(entry->text_id, buffer, &g_menu_text_state.pen);
            mem_copy_bytes_nonempty(g_text_substitution_values, g_world_text_substitution_values_backup,
                sizeof(g_text_substitution_values));
            LoadImage(&rect, buffer);
        }
        thread_yield();
        if (phase == 0) {
            menu_free_memory(buffer);
        }
        if (thread_get_current_parameter_3_variant() != 0) {
            break;
        }
        if (g_world_menu_use_alternate_main_palette != 0) {
            record->text_sprite.clut = 0x7D3C;
            record->frame_sprites[0].clut = 0x7C7C;
        } else {
            record->text_sprite.clut = 0x7C3C;
            record->frame_sprites[0].clut = 0x7CBC;
        }
        if (g_threads[g_thread_current_id].task_id == THREAD_TASK_MENU_GROUP_SECOND_WINDOW) {
            if (g_world_menu_main_scroll_threshold > 0x28 && target == 0) {
                if (g_world_menu_main_scroll_direction > 0) {
                    if (entry->window.x >= -0x3F) {
                        target = -0x78;
                    }
                } else if (entry->window.x < -0x3F) {
                    target = 0x78 - width;
                }
            }
            if (target != 0) {
                if (entry->window.x > target) {
                    entry->window.x -= 8;
                    if (entry->window.x <= target) {
                        entry->window.x = target;
                        target = 0;
                    }
                } else if (entry->window.x < target) {
                    entry->window.x += 8;
                    if (entry->window.x >= target) {
                        entry->window.x = target;
                        target = 0;
                    }
                }
            }
            record->text_sprite.x0 = entry->window.x;
        }
        setRGB0(&record->frame_sprites[0], g_world_menu_color_red, g_world_menu_color_green, g_world_menu_color_blue);
        gfx_draw_or_append_gpu_primitive(&record->frame_sprites[0]);
        gfx_draw_or_append_gpu_primitive(&record->frame_draw_mode);
        setRGB0(&record->text_sprite, g_world_menu_color_red, g_world_menu_color_green, g_world_menu_color_blue);
        gfx_draw_or_append_gpu_primitive(&record->text_sprite);
        gfx_draw_or_append_gpu_primitive(&record->text_draw_mode);
    }
    thread_yield();
    thread_exit_current();
}
