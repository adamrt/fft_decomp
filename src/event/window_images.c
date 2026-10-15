/* Byte copies, packed nibble access, 4bpp rect mirroring and pixel access, and window image and frame image building
 * and upload. */

#include "fft/battle.h"
#include "fft/battle_menu.h"
#include "fft/battle_text.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

static void gfx_mirror_4bpp_rect_variant(u8* pixels, const glyph_blit_t* rect);

/* Copy a nonempty byte sequence from source to destination.
 *
 * The target performs the first byte copy before testing the decremented
 * count, so callers must provide a positive byte count. */
void mem_copy_bytes_nonempty(void* destination, const void* source, s32 count) {
    /* Pin: unpinned, GCC copies the byte through $v0 instead of $a3. */
    register u8 value __asm__("$7");
    u8* destination_bytes = destination;
    const u8* source_bytes = source;

    /* The retail loop uses trapping addi updates, a nop after the lbu and an
     * unfilled bnez delay slot. GCC otherwise emits addiu and fills both. */
    do {
        value = *source_bytes;
        *destination_bytes = value;
        __asm__ volatile("addi %0,%0,1" : "=r"(destination_bytes) : "0"(destination_bytes));
        __asm__ volatile("addi %0,%0,1" : "=r"(source_bytes) : "0"(source_bytes));
        __asm__ volatile("addi %0,%0,-1" : "=r"(count) : "0"(count));
    } while (count);
}

#ifdef FFT_BATTLE
static s32 battle_gfx_read_packed_nibble(const u8* data, s32 index, s32 row, s32 stride) {
    s32 mask;
    s32 value;

    mask = index & 1;
    if (mask == 0) {
        mask = 0xF;
    } else {
        mask = 0xF0;
    }
    value = data[(row * stride + index) >> 1] & mask;
    if (mask == 0xF0) {
        value >>= 4;
    }
    return value;
}
#endif

#ifdef FFT_BATTLE
/* Write one nibble into a packed nibble array while preserving its neighbor.
 *
 * Matching note: the parity test lands in the mask's own register,
 * which is dead there because `data` has already been copied out of it, so the
 * parity has to be assigned to `mask` rather than tested inline; a fresh local
 * or an inline `if (!(index & 1))` puts the `andi` in the return register. */
static void battle_gfx_write_packed_nibble(u8* data, s32 index, s32 row, s32 stride, s32 value) {
    s32 mask;
    u8* destination;

    mask = index & 1;
    if (mask == 0) {
        mask = 0xf;
    } else {
        mask = 0xf0;
    }
    if (mask == 0xf0) {
        value <<= 4;
    }
    destination = data + ((row * stride + index) >> 1);
    *destination = (*destination & ~mask) | value;
}
#endif

#ifdef FFT_BATTLE
/* Mirrors a packed nibble rectangle horizontally, swapping each nibble with its opposite.
 * WORLD links its own body under this name, with the nibble helpers inlined. */
static void gfx_mirror_4bpp_rect_variant(u8* data, const glyph_blit_t* region) {
    s32 row;
    s32 col;
    s32 value;
    s32 offset;

    for (row = 0; row < region->rect.h; row++) {
        for (col = 0; col < region->rect.w / 2; col++) {
            value = battle_gfx_read_packed_nibble(data, region->rect.x + col, region->rect.y + row, region->stride);
            offset = 1;
            battle_gfx_write_packed_nibble(data, region->rect.x + col, region->rect.y + row, region->stride,
                battle_gfx_read_packed_nibble(
                    data, region->rect.x + region->rect.w - (col + offset), region->rect.y + row, region->stride));
            battle_gfx_write_packed_nibble(
                data, region->rect.x + region->rect.w - (col + offset), region->rect.y + row, region->stride, value);
        }
    }
}
#endif

#ifdef FFT_WORLD
/* The loop is an explicitly guarded do/while rather than a `for` because of
 * these placement facts:
 *  - `right = 1` is emitted between the loop's entry test and the loop top,
 *    which is only reachable when the initialization follows the guard's jump;
 *    with a `for`, the inits precede the duplicated exit test and the `li 1`
 *    is instead hoisted out of the row loop (a partial movable, with a
 *    compensating copy in the row loop's latch).
 *  - the guard is spelled `if (col < rect->w / 2)` after `col = 0;` rather
 *    than `if (rect->w / 2 > 0)`, so that `col = 0` precedes the guard branch
 *    (reorg puts it in the branch's delay slot) and `col` keeps the extra
 *    reference that makes it outrank `row` in the allocation order.
 *  - `right++` before `col++` puts `col++` in the reload's load shadow and
 *    `right++` in the loop branch's delay slot.
 */

/* Nibble accessors for a 4bpp image whose rows are `stride` pixels wide.
 * The target evaluates the argument expressions before choosing the mask
 * and multiplies afterwards, which is the shape of an inlined helper. */
static inline s32 world_get_4bpp_pixel(const u8* pixels, s32 x, s32 y, s32 stride) {
    s32 mask;
    s32 value;

    mask = x & 1;
    if (mask != 0) {
        mask = 0xF0;
    } else {
        mask = 0xF;
    }
    value = pixels[(y * stride + x) >> 1] & mask;
    if (mask == 0xF0) {
        value >>= 4;
    }
    return value;
}

static inline void world_set_4bpp_pixel(u8* pixels, s32 x, s32 y, s32 stride, s32 value) {
    s32 mask;
    u8* byte;

    mask = x & 1;
    if (mask != 0) {
        mask = 0xF0;
    } else {
        mask = 0xF;
    }
    if (mask == 0xF0) {
        value <<= 4;
    }
    byte = &pixels[(y * stride + x) >> 1];
    *byte = (*byte & ~mask) | value;
}

/* Mirrors a rectangle of a 4bpp image horizontally, swapping pixels from
 * both ends of each row toward the middle. BATTLE links its own body under
 * this name, calling out-of-line nibble helpers. */
static void gfx_mirror_4bpp_rect_variant(u8* pixels, const glyph_blit_t* rect) {
    s32 row;
    s32 col;
    s32 right;
    s32 left_value;
    s32 right_value;

    for (row = 0; row < rect->rect.h; row++) {
        col = 0;
        if (col < rect->rect.w / 2) {
            right = 1;
            do {
                left_value = world_get_4bpp_pixel(pixels, rect->rect.x + col, rect->rect.y + row, rect->stride);
                right_value = world_get_4bpp_pixel(
                    pixels, rect->rect.x + rect->rect.w - right, rect->rect.y + row, rect->stride);
                world_set_4bpp_pixel(pixels, rect->rect.x + col, rect->rect.y + row, rect->stride, right_value);
                world_set_4bpp_pixel(
                    pixels, rect->rect.x + rect->rect.w - right, rect->rect.y + row, rect->stride, left_value);
                right++;
                col++;
            } while (col < rect->rect.w / 2);
        }
    }
}
#endif

void menu_build_and_upload_window_image(s32 width, s32 height, RECT* rect, s32 mode, s32 tail_offset) {
    menu_build_window_image(width, height, rect, mode, tail_offset, 0);
}

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

#ifdef FFT_BATTLE
/* Builds a tiled window-frame image and optionally uploads it. Mode 2 may use
 * the fixed UI buffer at g_main_heap_companion_overlay_load_address + 0x8000. */
u8* menu_build_and_upload_window_frame_image_variant(s32 width, s32 height, RECT* rect, s32 mode) {
    RECT upload;
    s32 first;
    s32 words;
    s32 has_partial_word;
    s32 size;
    u16* src;
    u8* buffer;
    s32 i;
    s32 src_index;
    u16* row;

    upload.x = rect->x;
    upload.y = rect->y;
    upload.w = rect->w;
    upload.h = rect->h;
    if (width >= 0xF5) {
        width = 0xFF;
    }
    words = width & 3;
    has_partial_word = words != 0;
    words = (width >> 2) + has_partial_word;
    src = (u16*)g_menu_glyph_sheet->pixels;
    size = (width * height) >> 1;
    first = 0;
    if (size > 0x3000 && mode == MENU_WINDOW_FRAME_UPLOAD) {
        buffer = menu_alloc_memory(0x3000);
    } else if (mode == MENU_WINDOW_FRAME_ALTERNATE_UPLOAD && g_battle_menu_scroll_list_depth == 2) {
        buffer = g_main_heap_companion_overlay_load_address + 0x8000;
    } else {
        buffer = menu_alloc_memory(size);
    }
    for (i = 0; i < height; i++) {
        if (size >= 0x3000 && mode == MENU_WINDOW_FRAME_UPLOAD && (i & 0x5F) == 0) {
            if (i == 0) {
                upload.h = 0x60;
            } else {
                LoadImage(&upload, (u32*)buffer);
                thread_wait_frames(1);
                upload.y += 0x60;
                if (i - height >= 0x60) {
                    upload.h = 0x60;
                } else {
                    upload.h = height - i;
                }
                first += 0x60;
            }
        }
        if (mode == MENU_WINDOW_FRAME_ALTERNATE_UPLOAD || mode == MENU_WINDOW_FRAME_ALTERNATE_BUILD) {
            if (i < 0x10) {
                src_index = (i << 6) + 0x36;
            } else if (i >= height - 0x10) {
                src_index = (((i - height + 0x10) & 0xF) << 6) + 0x436;
            } else {
                src_index = (((i - 8) & 0xF) + 8) << 6;
            }
        } else {
            src_index = i << 6;
            if (i >= 8) {
                if (i >= height - 8) {
                    src_index = (((i - height + 8) & 7) << 6) + 0x600;
                } else {
                    src_index = (((i - 8) & 0xF) + 8) << 6;
                }
            }
        }
        row = (u16*)(buffer + (i - first) * words * sizeof(u16));
        row[words - 1] = src[src_index + 9];
        row[words - 2] = src[src_index + 8];
        menu_build_window_image_row(src, row, words, src_index);
    }
    if (mode == MENU_WINDOW_FRAME_UPLOAD || mode == MENU_WINDOW_FRAME_ALTERNATE_UPLOAD) {
        LoadImage(&upload, (u32*)buffer);
        thread_wait_frames(1);
        menu_free_memory(buffer);
    }
    return buffer;
}
#endif

#ifdef FFT_WORLD
/* Builds a tiled window-frame text image for rect in a UI buffer and, for
 * modes 0 and 2, uploads it (in 96-row bands when large) and frees it. Modes
 * 2 and 3 use the alternate frame pattern at 0x36/0x436. */
u8* menu_build_and_upload_window_frame_image_variant(s32 width, s32 height, RECT* rect, s32 mode) {
    RECT upload;
    s32 first;
    s32 words;
    s32 has_partial_word;
    u16* src;
    s32 size;
    u8* buffer;
    s32 i;
    s32 src_index;
    u16* row;

    upload.x = rect->x;
    upload.y = rect->y;
    upload.w = rect->w;
    upload.h = rect->h;
    if (width >= 0xF5) {
        width = 0xFF;
    }
    first = 0;
    words = width & 3;
    has_partial_word = words != 0;
    words = (width >> 2) + has_partial_word;
    src = (u16*)g_menu_glyph_sheet->pixels;
    size = (width * height) >> 1;
    if (size > 0x3000 && mode == MENU_WINDOW_FRAME_UPLOAD) {
        buffer = menu_alloc_memory(0x3000);
    } else {
        buffer = menu_alloc_memory(size);
    }
    for (i = 0; i < height; i++) {
        if (size >= 0x3000 && mode == MENU_WINDOW_FRAME_UPLOAD && (i & 0x5F) == 0) {
            if (i == 0) {
                upload.h = 0x60;
            } else {
                LoadImage(&upload, (u32*)buffer);
                thread_wait_frames(1);
                upload.y += 0x60;
                if (i - height >= 0x60) {
                    upload.h = 0x60;
                } else {
                    upload.h = height - i;
                }
                first += 0x60;
            }
        }
        if (mode == MENU_WINDOW_FRAME_ALTERNATE_UPLOAD || mode == MENU_WINDOW_FRAME_ALTERNATE_BUILD) {
            if (i < 0x10) {
                src_index = (i << 6) + 0x36;
            } else if (i >= height - 0x10) {
                src_index = (((i - height + 0x10) & 0xF) << 6) + 0x436;
            } else {
                src_index = (((i - 8) & 0xF) + 8) << 6;
            }
        } else {
            src_index = i << 6;
            if (i >= 8) {
                if (i >= height - 8) {
                    src_index = (((i - height + 8) & 7) << 6) + 0x600;
                } else {
                    src_index = (((i - 8) & 0xF) + 8) << 6;
                }
            }
        }
        row = (u16*)(buffer + (i - first) * words * sizeof(u16));
        row[words - 1] = src[src_index + 9];
        row[words - 2] = src[src_index + 8];
        menu_build_window_image_row(src, row, words, src_index);
    }
    if (mode == MENU_WINDOW_FRAME_UPLOAD || mode == MENU_WINDOW_FRAME_ALTERNATE_UPLOAD) {
        LoadImage(&upload, (u32*)buffer);
        thread_wait_frames(1);
        menu_free_memory(buffer);
    }
    return buffer;
}
#endif
