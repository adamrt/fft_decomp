#include "fft/battle.h"
#include "fft/battle_text.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "fft/thread.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Builds a tiled window-frame image and optionally uploads it. Mode 2 may use
 * the fixed UI buffer at g_event_overlay_load_address + 0x8000. */
void* battle_menu_build_and_upload_window_frame_image(s32 width, s32 height, RECT* rect, s32 mode) {
    RECT upload;
    s32 first;
    s32 words;
    s32 has_partial_word;
    s32 size;
    u16* src;
    u16* buffer;
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
    src = (u16*)g_battle_menu_glyph_image;
    size = (width * height) >> 1;
    first = 0;
    if (size > 0x3000 && mode == 0) {
        buffer = battle_menu_alloc_memory(0x3000);
    } else if (mode == 2 && g_battle_menu_scroll_list_depth == 2) {
        buffer = (u16*)(g_event_overlay_load_address + 0x8000);
    } else {
        buffer = battle_menu_alloc_memory(size);
    }
    for (i = 0; i < height; i++) {
        if (size >= 0x3000 && mode == 0 && (i & 0x5F) == 0) {
            if (i == 0) {
                upload.h = 0x60;
            } else {
                LoadImage(&upload, (u32*)buffer);
                battle_thread_wait_frames(1);
                upload.y += 0x60;
                if (i - height >= 0x60) {
                    upload.h = 0x60;
                } else {
                    upload.h = height - i;
                }
                first += 0x60;
            }
        }
        if (mode == 2 || mode == 3) {
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
        row = buffer + (i - first) * words;
        row[words - 1] = src[src_index + 9];
        row[words - 2] = src[src_index + 8];
        battle_menu_build_window_image_row(src, row, words, src_index);
    }
    if (mode == 0 || mode == 2) {
        LoadImage(&upload, (u32*)buffer);
        battle_thread_wait_frames(1);
        battle_menu_free_memory(buffer);
    }
    return buffer;
}
