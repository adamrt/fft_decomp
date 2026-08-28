#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Builds a tiled window-frame text image for rect in a UI buffer and, for
 * modes 0 and 2, uploads it (in 96-row bands when large) and frees it. Modes
 * 2 and 3 use the alternate frame pattern at 0x36/0x436. */
void* world_menu_build_and_upload_window_frame_image(s32 width, s32 height, RECT* rect, s32 mode) {
    RECT upload;
    s32 first;
    s32 words;
    s32 has_partial_word;
    u16* src;
    s32 size;
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
    first = 0;
    words = width & 3;
    has_partial_word = words != 0;
    words = (width >> 2) + has_partial_word;
    src = (u16*)g_world_menu_glyph_sheet;
    size = (width * height) >> 1;
    if (size > 0x3000 && mode == 0) {
        buffer = world_menu_alloc_ui_buffer(0x3000);
    } else {
        buffer = world_menu_alloc_ui_buffer(size);
    }
    for (i = 0; i < height; i++) {
        if (size >= 0x3000 && mode == 0 && (i & 0x5F) == 0) {
            if (i == 0) {
                upload.h = 0x60;
            } else {
                LoadImage(&upload, (u32*)buffer);
                world_thread_wait_frames(1);
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
        world_menu_fill_pattern_row(src, row, words, src_index);
    }
    if (mode == 0 || mode == 2) {
        LoadImage(&upload, (u32*)buffer);
        world_thread_wait_frames(1);
        world_menu_free_memory(buffer);
    }
    return buffer;
}
