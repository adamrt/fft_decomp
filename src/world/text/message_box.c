/* Message box slots and typewriter glyphs, quad flips, fixed-point lerps and window quad animations. */

#include "fft/gfx/gfx.h"
#include "fft/gfx/gfx_image.h"
#include "fft/menu/menu.h"
#include "fft/menu/menu_cursor.h"
#include "fft/menu/menu_gfx.h"
#include "fft/menu/menu_screen.h"
#include "fft/menu/menu_text.h"
#include "fft/menu/menu_window.h"
#include "fft/menu/option.h"
#include "fft/party/formation.h"
#include "fft/script/script.h"
#include "fft/script/script_variable.h"
#include "fft/sys/memory.h"
#include "fft/sys/sound.h"
#include "fft/sys/thread.h"
#include "fft/text/message.h"
#include "fft/text/text.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* message glyphs and staging slots */
extern text_glyph_state_t g_message_box_glyph;
static glyph_blit_t s_message_box_state;
static RECT s_message_box_vram_rect;
static u8 s_message_box_glyph_images[3][8][84];
static u8 s_message_box_previous_images[3][84];
static u8 s_message_box_column_scratch[14];

static s32 s_message_box_digit_divisors[9] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000 };
static s32 s_message_box_slot_threads[3] = { 0 };
static s32 s_message_box_slot_glyph_counters[3] = { 0 };
static s32 s_message_box_glyph_delay = 2;
static u8 s_message_box_slot_carry_offset[3] = { 0 };
static u8 s_message_box_slot_prev_width[3] = { 0 };

/* Render a message box's lines into its window image with the pen at (8, 8) and upload it; a
 * portrait box builds its tailed window image first, and centered boxes are skipped. */
static void render_box_text_image(text_dialog_record_t* record) {
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
        text_render_entry_to_image(record->text, record->buffer, &g_menu_window_text_pen);
        LoadImage(&record->rect, record->buffer);
        thread_yield();
        menu_free_memory(record->buffer);
    }
}

/* Free the message box text slot owned by the current thread, if any, and reset its glyph counter. */
static void release_box_slot(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (g_thread_current_id == s_message_box_slot_threads[i]) {
            s_message_box_slot_threads[i] = 0;
            s_message_box_slot_glyph_counters[i] = 0;
            return;
        }
    }
}

/* Draw the message box's pending glyph into VRAM and hold it for the typewriter delay.
 *
 * Cousin of message_draw_glyph_timed without the request pointer, task-id checks or
 * script counters. The ring byte is read before the phase and the inner bound
 * is `width / 2`, which keeps the per-row recomputation of the target.
 */
static void draw_box_glyph(s32 x, s32 y, s32 wait) {
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
            if (g_thread_current_id == s_message_box_slot_threads[i]) {
                slot = i;
                s_message_box_slot_threads[i] = g_thread_current_id;
                break;
            }
        }
        if (i == 3) {
            for (j = 0; j < 3; j++) {
                if (s_message_box_slot_threads[j] == 0) {
                    slot = j;
                    s_message_box_slot_threads[j] = g_thread_current_id;
                    s_message_box_slot_carry_offset[slot] = 0;
                    s_message_box_slot_prev_width[slot] = 0;
                    break;
                }
            }
        }
        if (slot != 0x10) {
            break;
        }
        thread_yield();
    }

    ring = s_message_box_slot_glyph_counters[slot];
    phase = g_message_box_glyph.x & 3;
    dst = s_message_box_glyph_images[slot][ring];
    src = s_message_box_previous_images[slot];

    /* VRAM x counts 4-pixel words: upload enough of them to cover the glyph from its phase in the first. */
    if (g_text_glyph_widths[g_message_box_glyph.glyph] == 4 || g_message_box_glyph.glyph == TEXT_SPACE) {
        if (phase == 2) {
            width = 8;
            s_message_box_vram_rect.w = 2;
        } else {
            width = 4;
            s_message_box_vram_rect.w = 1;
        }
    } else if (g_text_glyph_widths[g_message_box_glyph.glyph] == 2) {
        width = 4;
        s_message_box_vram_rect.w = 1;
    } else if (g_text_glyph_widths[g_message_box_glyph.glyph] == 6) {
        width = 8;
        s_message_box_vram_rect.w = 2;
    } else {
        width = 12;
        s_message_box_vram_rect.w = 3;
    }

    /* At phase 2 the first word is shared with the previous glyph: carry its byte over. Here and
     * below, offsetting the pointer before indexing keeps the original address arithmetic. */
    if (phase == 2) {
        for (i = 0; i < 14; i++) {
            s_message_box_column_scratch[i]
                = *(src + (i * s_message_box_slot_prev_width[slot]) / 2 + s_message_box_slot_carry_offset[slot]);
        }
        for (i = 0; i < 14; i++) {
            dst[i * width / 2] = s_message_box_column_scratch[i];
        }
    }

    /* Record which byte of this image the next glyph will carry over. */
    if (g_text_glyph_widths[g_message_box_glyph.glyph] == 4 || g_message_box_glyph.glyph == TEXT_SPACE) {
        if (phase == 2) {
            s_message_box_slot_carry_offset[slot] = 2;
        } else {
            s_message_box_slot_carry_offset[slot] = 1;
        }
    } else if (g_text_glyph_widths[g_message_box_glyph.glyph] == 2) {
        if (phase == 2) {
            s_message_box_slot_carry_offset[slot] = 1;
        } else {
            s_message_box_slot_carry_offset[slot] = 0;
        }
    } else if (g_text_glyph_widths[g_message_box_glyph.glyph] == 6) {
        if (phase == 2) {
            s_message_box_slot_carry_offset[slot] = 3;
        } else {
            s_message_box_slot_carry_offset[slot] = 2;
        }
    } else {
        if (phase == 2) {
            s_message_box_slot_carry_offset[slot] = 5;
        } else {
            s_message_box_slot_carry_offset[slot] = 4;
        }
    }
    s_message_box_slot_prev_width[slot] = width;

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
        s_message_box_state.rect.x = phase;
        s_message_box_state.rect.y = 0;
        s_message_box_state.stride = width;
        text_render_glyph_to_4bpp_image(g_text_glyph_bitmap_data + g_message_box_glyph.glyph * TEXT_GLYPH_BYTES, dst,
            &s_message_box_state, g_message_box_glyph.palette);
        s_message_box_vram_rect.x = (x >> 2) + 0x1c0;
        s_message_box_vram_rect.y = y;
        s_message_box_vram_rect.h = 14;
        LoadImage(&s_message_box_vram_rect, (u32*)dst);
    }
    /* Keep this image (14 rows of up to 12 pixels) for the next glyph's carry. */
    s_message_box_slot_glyph_counters[slot]++;
    mem_copy_bytes_nonempty(src, dst, 84);

    /* Wait 0 only syncs; 2 or more waits that many steps; 1 pauses after every few glyphs as the
     * message speed option sets (holding Circle counts as fast). */
    if (wait == 0) {
        DrawSync(0);
        s_message_box_slot_glyph_counters[slot] = 0;
    } else if (wait >= 2) {
        for (i = 0; i < wait * (3 - g_script_event_speed); i++) {
            thread_wait_frames(1);
        }
        s_message_box_slot_glyph_counters[slot] = 0;
    } else {
        if (PadRead(1) & PSX_PAD_CIRCLE) {
            s_message_box_glyph_delay = 8;
        } else if (g_game_options.fields.message_display_speed == GAME_MESSAGE_DISPLAY_SPEED_FAST) {
            s_message_box_glyph_delay = 8;
        } else if (g_game_options.fields.message_display_speed == GAME_MESSAGE_DISPLAY_SPEED_REGULAR) {
            s_message_box_glyph_delay = 2;
        } else {
            s_message_box_glyph_delay = 1;
        }
        if (s_message_box_slot_glyph_counters[slot] >= s_message_box_glyph_delay) {
            for (i = 0; i < 3 - g_script_event_speed; i++) {
                thread_wait_frames(1);
            }
            s_message_box_slot_glyph_counters[slot] = 0;
        }
    }

    if (g_message_box_glyph.mode != 0) {
        g_sound_pending_sfx_id = SOUND_SFX_TEXT_GLYPH;
    }
}

/* Mirror a POLY_FT4 texture horizontally when direction is negative: u0/u1
 * and u2/u3 trade places, each shifted by one texel. */
static void flip_box_quad(POLY_FT4* poly, s32 direction) {
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

/* The menu-screen library's copy of lerp_value_q12 (the same routine compiled twice). */
static s32 lerp_q12(s32 from, s32 to, s32 t) {
    return ((to - from) * t) / ONE + from;
}

/* Interpolate the four corners of a textured quad between two RECTs. The menu-screen library's copy of
 * lerp_rectangle_quad; animate_box_quads calls this one. */
static void lerp_quad(POLY_FT4* poly, const RECT* from, const RECT* to, s32 t) {
    setXY4(poly, lerp_q12(from->x, to->x, t), lerp_q12(from->y, to->y, t),
        lerp_q12(from->x + from->w, to->x + to->w, t), lerp_q12(from->y, to->y, t), lerp_q12(from->x, to->x, t),
        lerp_q12(from->y + from->h, to->y + to->h, t), lerp_q12(from->x + from->w, to->x + to->w, t),
        lerp_q12(from->y + from->h, to->y + to->h, t));
}

/*
 * Ease the current window's two quads from their present corners onto a pair
 * of destination rectangles. The interpolation parameter runs 0 -> 0x1000 in
 * decreasing steps, so the quads decelerate as they arrive; when the window is
 * growing (its quad area is smaller than the destination area) the sweep runs
 * a little past 0x1000 and then settles back down to it.
 */
static void animate_box_quads(menu_window_buffer_t* buffers, RECT* to_main, RECT* to_icon) {
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
            lerp_quad(&buffers[g_menu_packet_buffer_index].quads[0], &from_main, to_main, t);
            if (to_icon->x != 0) {
                lerp_quad(&buffers[g_menu_packet_buffer_index].quads[1], &from_icon, to_icon, t);
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
                lerp_quad(&buffers[g_menu_packet_buffer_index].quads[0], &from_main, to_main, t);
                if (to_icon->x != 0) {
                    lerp_quad(&buffers[g_menu_packet_buffer_index].quads[1], &from_icon, to_icon, t);
                }
            }
        }
    }

    t = ONE;
    for (frame = 0; frame < 3 - g_script_event_speed; frame++) {
        thread_yield();
        lerp_quad(&buffers[g_menu_packet_buffer_index].quads[0], &from_main, to_main, t);
        if (to_icon->x != 0) {
            lerp_quad(&buffers[g_menu_packet_buffer_index].quads[1], &from_icon, to_icon, t);
        }
    }
}

/*
 * Twin of menu_window_animate_crop without the two-pixel right
 * inset. Animate the current window's first quad growing out from the rectangle
 * center: each frame crops a sprite of the rectangle by the level's
 * percentage and copies the cropped bounds into the quad's corners.
 */
static void animate_box_crop(menu_window_buffer_t* buffers, RECT* rect, RECT* unused_to) {
    SPRT sprite;
    void* source; /* never assigned: a stale $s5 the callee never reads, as in menu_window_animate_crop */
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
        menu_zoom_sprite(source, &sprite, level);
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

    thread_current_set_task(THREAD_TASK_MESSAGE_BOX);
    value_index = 0;
    script_variable_base = EVENT_SCRIPT_VAR_SELECTED_DIALOG_OPTION;
    g_menu_screen_dialog_waiting = 0;
    param = thread_current_get_parameter_1();
    rec.box_type = param & 0x70;
    rec.dialog_type = param;
    rec.position = param & 3;
    rec.options = param & 0xC;
    text = text_find_entry(thread_current_get_parameter_2());
    base_entry = text_find_entry(thread_current_get_parameter_2() & 0xF800);

    /* Unlike a dialog, the box has a fixed size: 3 lines of 18 cells with a portrait, else 2 of 20. */
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        rec.mark = 18;
        rec.lines = 3;
    } else {
        rec.mark = 20;
        rec.lines = 2;
    }
    rec.height = rec.lines * 16 + 16;
    rec.width = rec.mark * 10 + 24;
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        rec.width = rec.mark * 10 + 64;
    }

    /* Center horizontally; positions 1, 2 and 3 are the top, bottom and middle. */
    rec.tail_offset = 0;
    half_w = rec.width >> 1;
    x = (256 - half_w) & ~1;
    half_h = rec.height >> 1;
    if (rec.position == 1) {
        y = 12;
    }
    if (rec.position == 2) {
        y = 228 - rec.height;
    }
    if (rec.position == 3) {
        y = (120 - half_h) & ~1;
    }
    right = x + half_w - 6;
    bottom = y + half_h - 2;

    /* A leading portrait command picks the portrait; otherwise the last one shown is kept. */
    win = menu_alloc_window_buffer_pair();
    win->message_id = thread_current_get_parameter_2();
    if (text[0] == TEXT_SET_PORTRAIT) {
        win[0].portrait_code = text[1];
        win[1].portrait_code = text[1];
        script_variable_set(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, text[1]);
    } else {
        win[0].portrait_code = script_variable_get(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT);
        win[1].portrait_code = script_variable_get(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT);
    }
    win[0].is_message_box = 1;
    win[1].is_message_box = 1;

    rec.text = thread_current_get_parameter_2();
    message = thread_current_get_parameter_2();
    rec.rect.w = rec.width;
    rec.rect.h = rec.height;
    gfx_alloc_texture_rect(&rec.rect, win->quads, (u32*)-1);
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        menu_build_and_upload_window_image(rec.width, rec.height, &rec.rect, rec.dialog_type, rec.tail_offset);
    } else {
        menu_build_and_upload_window_frame_image(rec.width, rec.height, &rec.rect, MENU_WINDOW_FRAME_UPLOAD);
    }

    /* The formation screen shifts its boxes by (-256, -120). */
    if (g_formation_screen_active != 0) {
        x -= 256;
        y -= 120;
        right -= 256;
        bottom -= 120;
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
        to.w = 31;
        to.h = 48;
        flip_box_quad(&win->quads[1], -1);
    } else {
        to.x = x + rec.width - 48;
        to.y = y + 8;
        to.w = 31;
        to.h = 48;
    }

    /* The pair's two buffers alternate by frame, so the second starts as a copy of the first. */
    mem_copy_bytes_nonempty(&win[1], &win[0], sizeof(win[1]));
    win[0].active = 1;
    win[1].active = 1;
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        animate_box_quads(win, &from, &to);
    } else {
        animate_box_crop(win, &from, &to);
    }

    /* From here x and y address the window's image in VRAM, not the screen. */
    x = win->quads[0].u0 + 8;
    y = win->quads[0].v0 + 8;
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT && rec.position == 1) {
        x = win->quads[0].u0 + 48;
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
        if (thread_current_get_parameter_2() != message) {
            if ((thread_current_get_parameter_2() & no_value) != no_value) {
                while (base_entry != text_find_entry(thread_current_get_parameter_2() & 0xF800)) {
                    thread_yield();
                }
            }
            message = thread_current_get_parameter_2();
            if ((message & no_value) == no_value) {
                g_menu_screen_dialog_waiting = 0;
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
            g_menu_screen_dialog_waiting = 0;
            width_override = no_value;
            rec.text = message;
            repeat = 0;
            if (text[0] == TEXT_SET_PORTRAIT) {
                win[0].portrait_code = text[1];
                win[1].portrait_code = text[1];
                script_variable_set(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, text[1]);
            } else {
                win[0].portrait_code = script_variable_get(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT);
                win[1].portrait_code = script_variable_get(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT);
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
                g_menu_screen_dialog_waiting = 1;
                /* The end of a substituted string returns to the text that named it. */
                if (saved != (u8*)-1) {
                    text = saved;
                    saved = (u8*)-1;
                    continue;
                }
                /* A break loop: a `while` is rotated to test at the bottom. */
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
                        if (thread_current_get_parameter_2() != message) {
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
                                    render_box_text_image(&rec);
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
                if (thread_current_get_parameter_2() != message) {
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
                cursor_y = y - 16;
                thread_yield();
            }

            no_wait = 0;
            cursor_x = x;
            cursor_y += 16;
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
            script_variable_set(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, *text);
            NEXT();
        } else if (code == TEXT_SET_VARIABLE_BASE) {
            NEXT();
            script_variable_base = *text;
            NEXT();
        } else if (code == TEXT_STORE_VARIABLE) {
            NEXT();
            i = script_variable_get(EVENT_SCRIPT_VAR_TEXT_STORE_INDEX);
            if (i != 0) {
                script_variable_set(script_variable_base + i * 10, *text);
                script_variable_set(EVENT_SCRIPT_VAR_TEXT_STORE_INDEX, 0);
            } else {
                script_variable_set(script_variable_base, *text);
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
            value = text_resolve_format_id(code, *substitution);
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
                    code = value / s_message_box_digit_divisors[i];
                    if (value < s_message_box_digit_divisors[i] && i != 0 && started == 0) {
                        code = TEXT_SPACE;
                    } else {
                        started = 1;
                        value -= code * s_message_box_digit_divisors[i];
                    }
                    g_message_box_glyph.glyph = code;
                    g_message_box_glyph.x = cursor_x;
                    g_message_box_glyph.y = cursor_y;
                    g_message_box_glyph.mode = rec.dialog_type & DIALOG_TYPE_BOX_TYPE_MASK;
                    g_message_box_glyph.palette = rec.color;
                    draw_box_glyph(cursor_x, cursor_y, delay);
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
                /* Built up one step at a time; the combined expressions are reassociated. */
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
                    code += *text;
                }

                g_message_box_glyph.glyph = code;
                g_message_box_glyph.mode = rec.box_type;
                g_message_box_glyph.palette = rec.color;
                g_message_box_glyph.x = cursor_x;
                g_message_box_glyph.y = cursor_y;
                draw_box_glyph(cursor_x, cursor_y, delay);
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
        animate_box_quads(win, &from, &to);
    }
    win->active = 0;
    menu_release_window_buffer_pair(win);
    gfx_free_texture_rect(&rec.rect);
    release_box_slot();
    thread_yield();
    thread_current_exit();
}
