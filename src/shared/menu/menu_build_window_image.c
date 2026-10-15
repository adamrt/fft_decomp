#include "fft/battle_menu.h"
#include "fft/battle_text.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/libgpu.h"

/* Build a 4bpp window and optional tail from the glyph sheet.
 * keep returns the allocated image; the upload/free path has no return value. */
u8* menu_build_window_image(s32 width, s32 height, RECT* rect, s32 mode, s32 tail_offset, s32 keep) {
    u16 kind = mode & DIALOG_TYPE_OPTIONS_MASK;
    s32 words;
    s32 partial;
    s32 byte_count;
    /* 16-bit row indexing preserves the original loop induction. */
    u16* buf;
    u16* src;
    s32 x_off;
    s32 i;
    s32 src_index;
    u16* row;

    words = width & 3;
    partial = words != 0;
    words = (width >> 2) + partial;
    byte_count = (width * (kind == DIALOG_OPTIONS_NO_TAIL ? height : height + 0x10)) >> 1;
    buf = menu_alloc_memory(byte_count);
    menu_clear_render_buffer(buf, byte_count);
    x_off = 0;
    src = (u16*)g_menu_glyph_sheet->pixels;
    if (kind != DIALOG_OPTIONS_NO_TAIL) {
        if ((mode & DIALOG_TYPE_POSITION_MASK) == 2) {
            x_off = words * 8;
        }
        if ((mode & DIALOG_TYPE_OPTIONS_MASK) == DIALOG_OPTIONS_TAIL) {
            height -= 8;
        }
        if ((mode & DIALOG_TYPE_OPTIONS_MASK) == DIALOG_OPTIONS_ALTERNATE_TAIL) {
            height -= 0x10;
        }
    }
    for (i = 0; i < height; i++) {
        if (i < 8) {
            src_index = (i << 6) + 0xA;
        } else if (i >= height - 8) {
            src_index = (((i - height + 8) & 7) << 6) + 0x60A;
        } else {
            src_index = ((((i - 8) & 0xF) + 8) << 6) + 0xA;
        }
        row = buf + i * words + x_off;
        row[words - 1] = 0;
        row[words - 2] = 0;
        menu_build_window_image_row(src, row, words, src_index);
    }
    if (kind != DIALOG_OPTIONS_NO_TAIL) {
        g_menu_window_sprite_glyph.rect.x = 0x58;
        g_menu_window_sprite_glyph.rect.y = 0;
        if ((mode & DIALOG_TYPE_OPTIONS_MASK) == DIALOG_OPTIONS_ALTERNATE_TAIL) {
            g_menu_window_sprite_glyph.rect.x = 0x68;
        }
        if ((mode & DIALOG_TYPE_POSITION_MASK) == 1) {
            g_menu_window_sprite_glyph.rect.y = 0x10;
        }
        g_menu_window_sprite_position.rect.x = (width >> 1) + tail_offset - 8;
        if ((mode & DIALOG_TYPE_POSITION_MASK) != 2) {
            g_menu_window_sprite_position.rect.y = 0x38;
        } else {
            g_menu_window_sprite_position.rect.y = 0;
        }
        g_menu_window_sprite_position.stride = words * 4;
        i = g_threads[g_thread_current_id].task_words[5] & 0xF0;
        if (i != 0) {
            if ((mode & DIALOG_TYPE_POSITION_MASK) == 1) {
                g_menu_window_sprite_position.rect.x -= 0x10;
            } else {
                g_menu_window_sprite_position.rect.x += 0x10;
            }
        }
        if (g_menu_window_sprite_position.rect.x < 0x10) {
            g_menu_window_sprite_position.rect.x = 0x10;
        }
        if (width - 0x10 < g_menu_window_sprite_position.rect.x) {
            g_menu_window_sprite_position.rect.x = width - 0x10;
        }
        text_blit_glyph(
            g_menu_glyph_sheet->pixels, (u8*)buf, &g_menu_window_sprite_glyph, &g_menu_window_sprite_position);
        if (i != 0) {
            gfx_mirror_4bpp_rect_variant((u8*)buf, &g_menu_window_sprite_position);
        }
    }
    if (keep != 0) {
        return (u8*)buf;
    }
    LoadImage(rect, (u32*)buf);
    thread_wait_frames(1);
    menu_free_memory(buf);
}
