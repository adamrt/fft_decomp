#include "fft/battle_event.h"
#include "fft/battle_menu.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

void menu_select_sprite_clut(void* packet) {
    s32 clut;
    texture_primitive_t* primitive = packet;

    if (thread_is_previous_running_variant() != 0 || g_menu_input_lock_mode == 1) {
        clut = 0x7D3C;
    } else {
        clut = 0x7C3C;
    }
    primitive->sprt.clut = clut;
}

/* Select a menu sprite's CLUT as battle_menu_select_icon_cluts does for its fourth
 * sprite: the dimmed one while the previous thread is running or g_menu_input_lock_mode is 1. */
void menu_select_icon_sprite_clut(SPRT* sprite) {
    s32 clut;

    if (thread_is_previous_running_variant() != 0 || g_menu_input_lock_mode == 1) {
        clut = 0x7c7c;
    } else {
        clut = 0x7cbc;
    }
    sprite->clut = clut;
}

/* Choose the CLUTs of the four menu-frame sprites: the dimmed set while the
 * previous thread is running or g_menu_input_lock_mode is 1. */
void menu_select_icon_cluts(menu_icon_sprites_t* frame) {
    if (thread_is_previous_running_variant() != 0 || g_menu_input_lock_mode == 1) {
        frame->sprites[0].clut = 0x7d3c;
        frame->sprites[1].clut = 0x7dfc;
        frame->sprites[2].clut = 0x7e3c;
        frame->sprites[3].clut = 0x7c7c;
    } else {
        frame->sprites[0].clut = 0x7c3c;
        frame->sprites[1].clut = 0x7d7c;
        frame->sprites[2].clut = 0x7dbc;
        frame->sprites[3].clut = 0x7cbc;
    }
}

/* Return the cursor's vertical animation offset for the current menu frame.
 *
 * The two signed threshold tables produce the idle and active cursor curves. */
s32 menu_get_cursor_bob_offset(s32 mode) {
    menu_cursor_bob_entry_t* entry;
    s32 counter;
    s32 threshold;

    counter = g_thread_contexts[g_thread_current_id].task_words[0];
    if (mode == 0) {
        entry = g_menu_cursor_bob_thresholds;
        counter %= entry->threshold;
    } else if (mode == 1) {
        entry = g_menu_cursor_bob_active_thresholds;
    }

    /* Any other mode leaves entry unset. The first threshold is tested here
     * and again by the loop condition. */
    entry++;
    threshold = entry->threshold;
    if (threshold == 0) {
        return 0;
    }
    while (threshold != 0) {
        if (counter < threshold) {
            return entry->offset;
        }
        entry++;
        threshold = entry->threshold;
    }
    return 0;
}

/* Position the icon-list cursor sprites and animate the highlighted icon.
 *
 * The current thread's 32-bit values at 0x50/0x54 hold the highlight timer and phase:
 * the phase restarts at 2 while input is blocked and clears once the timer
 * reaches 27 frames. The cursor sprites follow the selected row. */
void menu_update_icon_cursor_sprites(menu_entry_t* param, menu_icon_sprites_t* record, s32 frame, s32 cursor) {
    thread_t* thread = &g_threads[g_thread_current_id];
    s32 x;
    s32 y;

    if (cursor >= 0) {
        if (g_menu_formation_display_mode == 1) {
            thread->task_words[1] = 2;
            thread->task_words[0] = 0;
        }
        if (thread_is_previous_running_variant() == 0 && g_menu_input_lock_mode == 0) {
            if (thread->task_words[1] != 0 && thread->task_words[0] >= 0x1B) {
                thread->task_words[1] = 0;
                thread->task_words[0] = 0;
            }
        } else if (thread->task_words[1] == 0) {
            thread->task_words[1] = 2;
            thread->task_words[0] = 0;
        }
        if (thread->task_words[1] != 0) {
            x = menu_get_cursor_bob_offset(1);
        } else {
            x = menu_get_cursor_bob_offset(0);
        }
        thread->task_words[0] += g_script_event_speed;
        x -= 12;
        y = cursor * 16 + 10;
        record->sprites[1].x0 = param->window.x + x;
        record->sprites[1].y0 = param->window.y + y;
        record->sprites[2].x0 = param->window.x + x + 2;
        record->sprites[2].y0 = param->window.y + y + 2;
    }
    if (g_script_event_speed == 2) {
        frame *= 2;
    }
    menu_copy_sprite_position_uv_and_size(param, &record->sprites[0], frame);
    menu_zoom_cursor_frame(param, &record->sprites[0], frame);
}

/* Copy a menu entry's window position and text-image UV and size to its sprite. */
void menu_copy_sprite_position_uv_and_size(const menu_entry_t* entry, SPRT* sprite, s32 unused_frame) {
    sprite->x0 = entry->window.x;
    sprite->y0 = entry->window.y;
    sprite->u0 = (entry->vram_origin.x & 0x3f) * 4;
    sprite->v0 = entry->vram_origin.y;
    sprite->w = entry->text_image.width;
    sprite->h = entry->text_image.height;
}

/* Scale the sprite about its center and keep position and texture coordinates even. */
void menu_zoom_cursor_frame(const void* unused_source, SPRT* sprite, s32 step) {
    s32 scale;
    s32 offset;
    if (step < 12) {
        scale = g_menu_cursor_zoom_percentages[step];
        offset = sprite->w * scale / 200;
        sprite->x0 = (sprite->w / 2) + sprite->x0 - offset;
        sprite->u0 = (sprite->w / 2) + sprite->u0 - offset;
        sprite->x0 &= ~1;
        sprite->u0 &= ~1;
        offset = sprite->h * scale / 200;
        sprite->y0 = sprite->h / 2 + sprite->y0 - offset;
        sprite->w = sprite->w * scale / 100;
        sprite->v0 = sprite->h / 2 + sprite->v0 - offset;
        sprite->y0 &= ~1;
        sprite->v0 &= ~1;
        sprite->h = sprite->h * scale / 100;
        sprite->w &= 0xfe;
        sprite->h &= 0xfe;
    }
}

/*
 * Build a double-buffered draw-area pair whose rectangle is cropped to a
 * percentage of `geometry` about its center. Sources at x < 0x80 are moved
 * to the right-hand page (x + 0x80, y + 0x78); others are moved left.
 */
void gfx_build_scaled_draw_area_pair_swapped(
    scaled_draw_area_pair_t* packets, const RECT* geometry, s32 step, s32 flag) {
    s32 percent;
    s16 width;
    s16 height;
    s32 offset;

    if (g_script_event_speed == 2) {
        step *= 2;
    }
    if (step >= 12) {
        step = 11;
    }
    percent = g_menu_cursor_zoom_percentages[step];
    mem_copy_bytes_nonempty(&packets->rects[0], geometry, sizeof(*geometry));
    if (packets->rects[0].x < 0x80) {
        packets->rects[0].x += 0x80;
        packets->rects[0].y += 0x78;
    } else {
        packets->rects[0].x -= 0x80;
    }
    width = packets->rects[0].w;
    height = packets->rects[0].h;
    offset = width * percent / 200;
    packets->rects[0].x = width / 2 + packets->rects[0].x - offset;
    offset = height * percent / 200;
    packets->rects[0].y = height / 2 + packets->rects[0].y - offset;
    packets->rects[0].w = (packets->rects[0].w * percent) / 100;
    packets->rects[0].h = (packets->rects[0].h * percent) / 100;
    if (flag != 0) {
        packets->rects[1].y = 0;
    } else {
        packets->rects[1].y = 0xF0;
        packets->rects[0].y += 0xF0;
    }
    SetDrawArea(&packets->areas[0], &packets->rects[0]);
    SetDrawArea(&packets->areas[1], &packets->rects[1]);
}
