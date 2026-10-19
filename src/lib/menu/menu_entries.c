/* Menu-entry actions and input gating, icon-strip rendering and cursor animation. */

#include "fft/command.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/help.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_window.h"
#include "fft/screen.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Twelve crop percentages, indexed by step. */
static const u16 s_menu_cursor_zoom_percentages[12] = { 10, 10, 60, 60, 90, 90, 95, 95, 100, 100, 100, 100 };

/* Four pieces for each header from MENU_ICON_HEADER_ABILITY_TURN_REFLECT through
 * MENU_ICON_HEADER_ABILITY, followed by the five single-icon header layouts. */
static image_layout_t s_menu_icon_strip_image_params[20]
    = { { { 0, 32, 26, 10 }, 12, -1 }, { { 156, 176, 22, 8 }, 107, -1 }, { { 184, 32, 16, 10 }, -415, -1 },
          { { 178, 176, 18, 8 }, 88, -1 }, { { 0, 32, 26, 10 }, 13, -1 }, { { 184, 32, 16, 10 }, 107, -1 },
          { { 156, 176, 22, 8 }, 123, -1 }, { { 178, 176, 18, 8 }, 86, -1 }, { { 0, 32, 26, 10 }, -512, -1 },
          { { 0, 152, 24, 8 }, 106, -1 }, { { 156, 176, 22, 8 }, 132, -1 }, { { 178, 176, 18, 8 }, 89, -1 },
          { { 0, 32, 26, 10 }, -512, -1 }, { { 156, 176, 22, 8 }, 107, -1 }, { { 184, 32, 16, 10 }, -415, -1 },
          { { 178, 176, 18, 8 }, 88, -1 }, { { 0, 32, 26, 10 }, 12, -1 }, { { 156, 176, 22, 8 }, 512, -1 },
          { { 184, 32, 16, 10 }, 512, -1 }, { { 178, 176, 18, 8 }, 512, -1 } };
static image_layout_t s_menu_single_icon_image_params[5]
    = { { { 0, 32, 26, 10 }, -512, -1 }, { { 120, 120, 24, 8 }, 3, -1 }, { { 64, 120, 26, 8 }, 3, -1 },
          { { 92, 120, 28, 8 }, 3, -1 }, { { 156, 176, 22, 8 }, 3, -1 } };

/* One cursor animation threshold and its signed screen offset. */
typedef struct menu_cursor_bob_entry {
    s8 threshold;
    s8 offset;
} menu_cursor_bob_entry_t;
typedef char menu_cursor_bob_entry_size_must_be_2[sizeof(menu_cursor_bob_entry_t) == 2 ? 1 : -1];

/* Idle and active cursor curves end at threshold 0. The idle curve uses entry 0
 * as its modulus; the active curve skips its first entry. */
static menu_cursor_bob_entry_t s_menu_cursor_bob_thresholds[8]
    = { { 46, 0 }, { 16, 0 }, { 18, 1 }, { 22, 2 }, { 28, 3 }, { 36, 2 }, { 46, 1 }, { 0, 0 } };
static menu_cursor_bob_entry_t s_menu_cursor_bob_active_thresholds[13] = { { 38, 0 }, { 2, 0 }, { 8, -2 }, { 10, 1 },
    { 12, 3 }, { 14, 5 }, { 16, 4 }, { 18, 3 }, { 24, 2 }, { 34, 1 }, { 36, 0 }, { 38, 0 }, { 0, 0 } };

static void menu_copy_sprite_position_uv_and_size(const menu_entry_t* entry, SPRT* sprite, s32 unused_frame);

/* Returns 1 when a menu thread may act on input: no previous event thread
 * running, no third thread parameter (a stop request), not in event mode, no
 * help menu open, and g_menu_input_block_frames clear. In WORLD, a help box
 * (box type 0x30) in the text or message slot blocks input first and arms a
 * five-frame block. */
s32 menu_can_accept_input(void) {
#ifdef FFT_WORLD
    if ((thread_is_running_asm(THREAD_SCREEN_SLOT_TEXT)
            && (g_thread_contexts[THREAD_SCREEN_SLOT_TEXT].function_parameter_1 & 0x70) == 0x30)
        || (thread_is_running_asm(THREAD_SCREEN_SLOT_MESSAGE)
            && (g_thread_contexts[THREAD_SCREEN_SLOT_MESSAGE].function_parameter_1 & 0x70) == 0x30)) {
        g_menu_input_block_frames = 5;
        return 0;
    }
#endif
    if (thread_is_previous_running() != 0 || thread_get_current_parameter_3() != 0 || g_menu_input_lock_mode != 0
        || g_help_open != 0 || g_menu_input_block_frames != 0) {
        return 0;
    }
    return 1;
}

/* Act on Circle for menu row row_index: start the row's menu thread one slot below the current
 * one, clear the selection, or for a negative action -n ask n menu threads from the current one up
 * to close. Stores the row as the entry's result and plays the confirm sound unless the action is
 * silent; in tutorial input mode rows disabled by EVENT_SCRIPT_VAR_DISABLED_MENU_ACTIONS do
 * nothing. */
void menu_handle_action(menu_entry_t* entry, s32 row_index) {
    s32 silent;
    s32 target;
    s32 i;
    s32 j;
    s32 thread_id;
    s32 menu_thread_id;
    s16* row_actions;
    s16* result;

    if (g_menu_input_mode == MENU_INPUT_TUTORIAL
        && ((script_get_variable(EVENT_SCRIPT_VAR_DISABLED_MENU_ACTIONS) >> row_index) & 1)) {
        return;
    }
    silent = 0;
    if ((g_script_event_input & PSX_PAD_CIRCLE) && menu_can_accept_input() != 0) {
        if (g_thread_contexts[g_thread_current_id].task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] == 0) {
            g_thread_contexts[g_thread_current_id].task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] = 2;
            g_thread_contexts[g_thread_current_id].task_words[MENU_CURSOR_THREAD_WORD_BOB_TIMER] = 0;
        }
        row_actions = entry->row_actions;
        target = ((s32)row_actions >= -0xf && (s32)row_actions <= -1) ? (s32)row_actions : row_actions[row_index];
        if (target >= 0) {
            if (target & MENU_ROW_ACTION_SILENT) {
                silent = 1;
                /* Raises silent's allocation priority; otherwise $s6-$s8 rotate. */
                __asm__("" : : "r"(silent), "r"(silent));
                target -= MENU_ROW_ACTION_SILENT;
            }
        }
        if (target >= 0) {
            if (target < 0x100) {
                thread_start(g_thread_current_id - 1, g_menu_thread_menu_data[target].thread_entry);
                thread_set_parameters(g_thread_current_id - 1, (s32)&g_menu_thread_menu_data[target], 0, 0);
            } else if (target == MENU_ROW_ACTION_CLEAR_SELECTION) {
                row_index = -1;
            }
        } else {
            target = -target;
            for (i = 0; i < VARIANT_THREAD_TABLE_SLOT_COUNT; i++) {
                if (g_thread_contexts[i].task_id == THREAD_TASK_RESUME) {
                    g_thread_contexts[i].task_id = THREAD_TASK_STOP_REQUEST;
                }
            }
            /* Remember the rows of the closing battle menus (the menu slot and its
             * children below it) as the pending selection. */
            for (i = 0; i < target; i++) {
                thread_id = i + g_thread_current_id;
                g_thread_contexts[thread_id].function_parameter_3 = 1;
                if (thread_id == THREAD_SLOT_MENU) {
                    for (j = 0; j < target; j++) {
                        menu_thread_id = THREAD_SLOT_MENU - j;
                        if (thread_is_running_asm(menu_thread_id) != 0) {
                            g_command_pending_selection[j]
                                = ((menu_entry_t*)g_thread_contexts[menu_thread_id].function_parameter_1)
                                      ->selected_index;
                        }
                    }
                }
            }
        }
        g_script_event_input = 0;
        result = entry->value;
        *result = row_index;
        if (g_sound_suppressed == 0 && silent == 0) {
            sound_queue_sfx_confirm();
        }
    }
}

/* While the menu may accept input, move a row cursor with Up and Down, wrapping
 * between row 0 and the entry's max_row_index, and play the cursor sound. */
void menu_step_wrapping_cursor_on_scroll_buttons(menu_entry_t* ctx, s32* sel) {
    s32 selection;
    s32 max;
    s32 cur;

    if (menu_can_accept_input() != 0) {
        if (g_script_event_input & PSX_PAD_UP) {
            selection = *sel;
            if (selection == 0) {
                selection = ctx->max_row_index;
            } else {
                selection = selection - 1;
            }
            *sel = selection;
            sound_queue_sfx_cursor_move();
        }
        if (g_script_event_input & PSX_PAD_DOWN) {
            max = ctx->max_row_index;
            cur = *sel;
            if (cur == max) {
                *sel = 0;
            } else {
                *sel = cur + 1;
            }
            sound_queue_sfx_cursor_move();
        }
    }
}

/* On the cancel button, flag the context's thread group as finished, turn every
 * THREAD_TASK_RESUME thread into a stop request and report -1 to the caller. Closing the
 * battle menu slot also forgets the pending selection. Menu input is then held off for 5
 * frames. */
void menu_handle_cancel_input(menu_entry_t* context) {
    s32 i;
    s32 j;
    s32 thread_id;
    s32 count;
    s16* result;

    if (!(g_script_event_input & PSX_PAD_CROSS)) {
        return;
    }
    if (menu_can_accept_input() == 0) {
        return;
    }
    count = context->thread_count;
    if (count != -1) {
        for (i = 0; i < count; i++) {
            thread_id = i + g_thread_current_id;
            g_thread_contexts[thread_id].function_parameter_3 = 1;
            if (thread_id == THREAD_SLOT_MENU) {
                for (j = 3; j >= 0; j--) {
                    g_command_pending_selection[j] = -1;
                }
            }
        }
        for (i = 0; i < VARIANT_THREAD_TABLE_SLOT_COUNT; i++) {
            if (g_thread_contexts[i].task_id == THREAD_TASK_RESUME) {
                g_thread_contexts[i].task_id = THREAD_TASK_STOP_REQUEST;
            }
        }
    }
    g_script_event_input = 0;
    result = context->value;
    *result = -1;
    sound_queue_sfx_cancel();
    g_script_input_suppression_frames = 5;
}

/* Suppress menu input while another thread has a stop request and report
 * completion: 1 once no other running thread has a stop request and this
 * thread's parameter 3 is set. In WORLD, SELECT first opens the entry's
 * auxiliary text thread when its table selector is nonzero. */
s32 menu_should_close_thread(u32* input) {
#ifdef FFT_WORLD
    menu_entry_t* parameter;
#endif
    s32 i;

#ifdef FFT_WORLD
    if (g_thread_contexts[THREAD_SCREEN_SLOT_TEXT].is_running == 0 && menu_can_accept_input() != 0
        && (*input & PSX_PAD_SELECT)) {
        parameter = (menu_entry_t*)thread_get_current_parameter_1();
        if (parameter->select_text_table != 0) {
            if (g_script_input_suppression_frames == 0) {
                thread_start(THREAD_SCREEN_SLOT_TEXT, screen_help_text_thread);
                thread_set_parameters(THREAD_SCREEN_SLOT_TEXT, (s32)parameter, 0, 0);
                g_menu_input_lock_mode = 1;
            }
            *input = 0;
        }
    }
#endif
    if (g_menu_input_lock_mode != 0 || g_help_open != 0) {
        *input = 0;
        return 0;
    }
    for (i = 0; i < VARIANT_THREAD_TABLE_SLOT_COUNT; i++) {
        if (i != g_thread_current_id && thread_is_running_asm(i) != 0
            && g_thread_contexts[i].task_id == THREAD_TASK_STOP_REQUEST) {
            *input = 0;
            break;
        }
    }
    if (i == VARIANT_THREAD_TABLE_SLOT_COUNT && thread_get_current_parameter_3() != 0) {
        return 1;
    }
    return 0;
}

/* Publish the icon strip's text-image upload rectangle to the caller, build
 * the four menu sprites of the icon record, and place the icon sprites that
 * the strip's header selects. Only WORLD has the Menu header with an arrow. */
void menu_build_window_sprites(RECT* rect, menu_entry_t* param, menu_icon_record_t* record) {
    s32 kind;
    s32 index;
    /* Keep this halfword value separate from integer header-ID constants
     * to preserve the original register allocation. */
    u16 size;
    /* Eight untouched bytes preserve the original 0x40-byte stack frame. */
    u8 unused_scratch[8];

    thread_set_current_task_id(THREAD_TASK_MENU_WINDOW_BUILD);
    rect->x = param->vram_origin.x;
    rect->y = param->vram_origin.y;
    rect->w = param->inner_width >> 2;
    rect->h = param->inner_height;
    record->_unknown_74 = 0;
    record->_unknown_78 = 0;
    menu_init_sprite_array(record->base.sprites, 4, 0x7cfc);
    gfx_set_image_primitive_layout(record->base.sprites, &param->vram_origin, &param->window, &param->text_image);
    SetSemiTrans(&record->base.sprites[1], 0);
    SetSemiTrans(&record->base.sprites[2], 1);
    size = 0x10;
    record->base.sprites[1].w = size;
    record->base.sprites[1].h = size;
    record->base.sprites[2].w = size;
    record->base.sprites[2].h = size;
    record->base.sprites[1].u0 = 0xa8;
    record->base.sprites[1].v0 = 0;
    record->base.sprites[2].u0 = 0xb8;
    record->base.sprites[2].v0 = 0;
    kind = param->header_id;
    /* The range test below reads its own 16-bit copy of the kind. */
    index = (s16)kind;
#ifdef FFT_WORLD
    if (kind == MENU_ICON_HEADER_MENU_WITH_ARROW) {
        const image_layout_t* image_params;

        menu_init_sprite_array(record->icons[0], 1, 0x7cfc);
        record->icons[0]->u0 = 0x30;
        record->icons[0]->v0 = 0x10;
        record->icons[0]->w = size;
        record->icons[0]->h = 8;
        image_params = param->context;
        record->icons[0]->x0 = image_params->source.h + param->window.x + 7;
        record->icons[0]->y0 = param->window.y - 1;
        gfx_set_image_primitive_layout(&record->base.sprites[3], &g_menu_texture_origin, &param->window,
            &s_menu_single_icon_image_params[MENU_ICON_HEADER_MENU]);
    } else
#endif
        if (kind < 5) {
        gfx_set_image_primitive_layout(
            &record->base.sprites[3], &g_menu_texture_origin, &param->window, &s_menu_single_icon_image_params[kind]);
    } else {
        /* The native test also accepts 10 through 15, beyond the five header groups. */
        if (index >= 5 && index < 0x10) {
            menu_init_sprite_array(record->icons[0], 1, 0x7cfc);
            menu_init_sprite_array(record->icons[1], 1, 0x7cfc);
            menu_init_sprite_array(record->icons[2], 1, 0x7cfc);
            /* `kind` stays unscaled and the x4 is spelled `<< 2` inline. Written
             * `kind * 4 + n`, fold collapses it to one x48 whose 3-op synth is an
             * instruction short; `(kind << 2) + n` selects the target's 4-op form. */
            kind -= 5;
            gfx_set_image_primitive_layout(&record->base.sprites[3], &g_menu_texture_origin, &param->window,
                &s_menu_icon_strip_image_params[(kind << 2)]);
            gfx_set_image_primitive_layout(record->icons[0], &g_menu_texture_origin, &param->window,
                &s_menu_icon_strip_image_params[(kind << 2) + 1]);
            gfx_set_image_primitive_layout(record->icons[1], &g_menu_texture_origin, &param->window,
                &s_menu_icon_strip_image_params[(kind << 2) + 2]);
            gfx_set_image_primitive_layout(record->icons[2], &g_menu_texture_origin, &param->window,
                &s_menu_icon_strip_image_params[(kind << 2) + 3]);
        } else {
            record->icons[0] = 0;
            record->icons[1] = 0;
            record->icons[2] = 0;
        }
    }
    gfx_set_draw_mode_from_origin(&record->base.first_draw_mode, &param->vram_origin);
    SetDrawMode(&record->base.draw_mode, 0, 0, GetTPage(0, 2, 0x3c0, 0x100), &g_gfx_texture_window);
}

/* Give a menu sprite or quad the dimmed menu CLUT while the previous thread is running or
 * g_menu_input_lock_mode is 1, else the normal one. */
void menu_select_sprite_clut(void* packet) {
    s32 clut;
    texture_primitive_t* primitive = packet;

    if (thread_is_previous_running() != 0 || g_menu_input_lock_mode == 1) {
        clut = MENU_CLUT_DIMMED;
    } else {
        clut = MENU_CLUT_NORMAL;
    }
    primitive->sprt.clut = clut;
}

/* Select a menu sprite's CLUT as menu_select_icon_cluts does for its fourth
 * sprite: the dimmed one while the previous thread is running or g_menu_input_lock_mode is 1. */
void menu_select_icon_sprite_clut(SPRT* sprite) {
    s32 clut;

    if (thread_is_previous_running() != 0 || g_menu_input_lock_mode == 1) {
        clut = 0x7c7c;
    } else {
        clut = 0x7cbc;
    }
    sprite->clut = clut;
}

/* Choose the CLUTs of the four menu-frame sprites: the dimmed set while the
 * previous thread is running or g_menu_input_lock_mode is 1. */
void menu_select_icon_cluts(menu_icon_sprites_t* frame) {
    if (thread_is_previous_running() != 0 || g_menu_input_lock_mode == 1) {
        frame->sprites[0].clut = MENU_CLUT_DIMMED;
        frame->sprites[1].clut = 0x7dfc;
        frame->sprites[2].clut = 0x7e3c;
        frame->sprites[3].clut = 0x7c7c;
    } else {
        frame->sprites[0].clut = MENU_CLUT_NORMAL;
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
    s32 counter = g_thread_contexts[g_thread_current_id].task_words[MENU_CURSOR_THREAD_WORD_BOB_TIMER];
    s32 threshold;

    if (mode == 0) {
        entry = s_menu_cursor_bob_thresholds;
        counter %= entry->threshold;
    } else if (mode == 1) {
        entry = s_menu_cursor_bob_active_thresholds;
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
 * The current thread's cursor bob task words hold the timer and the active
 * flag: the flag restarts at 2 while input is blocked and clears once the
 * timer reaches 27. The cursor sprites follow the selected row. */
void menu_update_icon_cursor_sprites(menu_entry_t* param, menu_icon_sprites_t* record, s32 frame, s32 cursor) {
    thread_t* thread = &g_threads[g_thread_current_id];
    s32 x;
    s32 y;

    if (cursor >= 0) {
        if (g_menu_formation_display_mode == 1) {
            thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] = 2;
            thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_TIMER] = 0;
        }
        if (thread_is_previous_running() == 0 && g_menu_input_lock_mode == 0) {
            if (thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] != 0
                && thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_TIMER] >= 0x1B) {
                thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] = 0;
                thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_TIMER] = 0;
            }
        } else if (thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] == 0) {
            thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] = 2;
            thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_TIMER] = 0;
        }
        if (thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] != 0) {
            x = menu_get_cursor_bob_offset(1);
        } else {
            x = menu_get_cursor_bob_offset(0);
        }
        thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_TIMER] += g_script_event_speed;
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
static void menu_copy_sprite_position_uv_and_size(const menu_entry_t* entry, SPRT* sprite, s32 unused_frame) {
    sprite->x0 = entry->window.x;
    sprite->y0 = entry->window.y;
    sprite->u0 = (entry->vram_origin.x & 0x3f) * 4;
    sprite->v0 = entry->vram_origin.y;
    sprite->w = entry->text_image.source.w;
    sprite->h = entry->text_image.source.h;
}

/* Scale the sprite about its center by the percentage of zoom step `step` (0-11; later steps
 * leave it alone), keeping position, texture coordinates and size even. */
void menu_zoom_cursor_frame(const void* unused_source, SPRT* sprite, s32 step) {
    s32 scale;
    s32 offset;
    if (step < 12) {
        scale = s_menu_cursor_zoom_percentages[step];
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
 * Nonzero upper_half selects the buffer at y 0; zero selects y 240.
 */
void gfx_build_scaled_draw_area_pair_swapped(
    scaled_draw_area_pair_t* packets, const RECT* geometry, s32 step, s32 upper_half) {
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
    percent = s_menu_cursor_zoom_percentages[step];
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
    if (upper_half != 0) {
        packets->rects[1].y = 0;
    } else {
        packets->rects[1].y = 0xF0;
        packets->rects[0].y += 0xF0;
    }
    SetDrawArea(&packets->areas[0], &packets->rects[0]);
    SetDrawArea(&packets->areas[1], &packets->rects[1]);
}
