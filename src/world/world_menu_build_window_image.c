#include "fft/world.h"
#include "psx/gpu.h"

/* Build a 4bpp menu window image from the shared menu graphics, optionally
 * with its pointer tail, and upload it to `rect` unless `keep` is set.
 *
 * `mode` bits 2-3 select the frame style (8 = plain frame, no tail); bits 0-1
 * place the tail (1 and 2 flip it vertically). The tail is drawn through
 * world_text_blit_glyph from the fixed source/destination records at
 * 0x80156c40/0x80156c50, and nudged 16 pixels when the current thread's
 * task_words[5] has any of bits 4-7 set.
 *
 * With `keep` set the caller owns the returned buffer. Otherwise the buffer
 * is freed and the result is whatever world_menu_free_memory left in $v0;
 * the target has no return value on that path. `i` is reused for the thread
 * flag because a separate local swaps the $s0/$s1 allocation. */
void* world_menu_build_window_image(s32 width, s32 height, RECT* rect, s32 mode, s32 tail_offset, s32 keep) {
    u16 kind = mode & 0xC;
    s32 words;
    s32 partial;
    s32 byte_count;
    u16* image;
    u16* sheet;
    s32 x_offset;
    s32 i;
    s32 sheet_offset;
    u16* row;

    words = width & 3;
    partial = words != 0;
    words = (width >> 2) + partial;
    byte_count = (width * (kind == 8 ? height : height + 0x10)) >> 1;
    image = world_menu_alloc_ui_buffer(byte_count);
    world_clear_menu_render_buffer((u8*)image, byte_count);
    x_offset = 0;
    sheet = (u16*)g_world_menu_glyph_sheet;
    if (kind != 8) {
        if ((mode & 3) == 2) {
            x_offset = words * 8;
        }
        if ((mode & 0xC) == 0) {
            height -= 8;
        }
        if ((mode & 0xC) == 4) {
            height -= 0x10;
        }
    }
    for (i = 0; i < height; i++) {
        if (i < 8) {
            sheet_offset = (i << 6) + 0xA;
        } else if (i >= height - 8) {
            sheet_offset = (((i - height + 8) & 7) << 6) + 0x60A;
        } else {
            sheet_offset = ((((i - 8) & 0xF) + 8) << 6) + 0xA;
        }
        row = image + i * words + x_offset;
        row[words - 1] = 0;
        row[words - 2] = 0;
        world_menu_fill_pattern_row(sheet, row, words, sheet_offset);
    }
    if (kind != 8) {
        g_world_menu_window_sprite_source.rect.x = 0x58;
        g_world_menu_window_sprite_source.rect.y = 0;
        if ((mode & 0xC) == 4) {
            g_world_menu_window_sprite_source.rect.x = 0x68;
        }
        if ((mode & 3) == 1) {
            g_world_menu_window_sprite_source.rect.y = 0x10;
        }
        g_world_menu_window_sprite_position.rect.x = (width >> 1) + tail_offset - 8;
        if ((mode & 3) != 2) {
            g_world_menu_window_sprite_position.rect.y = 0x38;
        } else {
            g_world_menu_window_sprite_position.rect.y = 0;
        }
        g_world_menu_window_sprite_position.stride = words * 4;
        i = g_world_threads[g_world_thread_current_id].task_words[5] & 0xF0;
        if (i != 0) {
            if ((mode & 3) == 1) {
                g_world_menu_window_sprite_position.rect.x -= 0x10;
            } else {
                g_world_menu_window_sprite_position.rect.x += 0x10;
            }
        }
        if (g_world_menu_window_sprite_position.rect.x < 0x10) {
            g_world_menu_window_sprite_position.rect.x = 0x10;
        }
        if (width - 0x10 < g_world_menu_window_sprite_position.rect.x) {
            g_world_menu_window_sprite_position.rect.x = width - 0x10;
        }
        world_text_blit_glyph((u16*)g_world_menu_glyph_sheet, image, &g_world_menu_window_sprite_source,
            &g_world_menu_window_sprite_position);
        if (i != 0) {
            world_gfx_mirror_4bpp_rect((u8*)image, (world_gfx_4bpp_rect_t*)&g_world_menu_window_sprite_position);
        }
    }
    if (keep != 0) {
        return image;
    }
    LoadImage(rect, (u32*)image);
    world_thread_wait_frames(1);
    world_menu_free_memory(image);
}
