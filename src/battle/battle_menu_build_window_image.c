#include "fft/battle.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Provisional: sprite source/destination records passed to blit_text_glyph. */
typedef struct {
    s16 x; /* 0x00 */
    s16 y; /* 0x02 */
    u8 unknown_04[4];
    s32 stride; /* 0x08 */
    u8 unknown_0c[4];
} battle_menu_window_sprite_pos_t;

extern battle_menu_window_sprite_pos_t g_battle_menu_window_sprite_glyph;
extern battle_menu_window_sprite_pos_t g_battle_menu_window_sprite_position;

/* Battle twin of world_menu_build_window_image: build a 4bpp menu window image from the shared menu graphics,
 * optionally with its pointer tail, and upload it to `rect` unless `keep` is set.
 *
 * `mode` bits 2-3 select the frame style (8 = plain frame, no tail); bits 0-1
 * place the tail (1 and 2 flip it vertically). The tail is drawn through
 * blit_text_glyph from the fixed source/destination records at
 * 0x801697b0/0x801697c0, and nudged 16 pixels when the current thread's
 * task_words[5] has any of bits 4-7 set.
 *
 * With `keep` set the caller owns the returned buffer. Otherwise the buffer
 * is freed and the result is whatever battle_menu_free_memory left in $v0;
 * the target has no return value on that path. `i` is reused for the thread
 * flag because a separate local swaps the $s0/$s1 allocation. */
void* battle_menu_build_window_image(s32 width, s32 height, RECT* rect, s32 mode, s32 tail_offset, s32 keep) {
    u16 kind = mode & 0xC;
    s32 words;
    s32 partial;
    s32 byte_count;
    u16* buf;
    u16* src;
    s32 x_off;
    s32 i;
    s32 src_index;
    u16* row;

    words = width & 3;
    partial = words != 0;
    words = (width >> 2) + partial;
    byte_count = (width * (kind == 8 ? height : height + 0x10)) >> 1;
    buf = battle_menu_alloc_memory(byte_count);
    battle_clear_menu_render_buffer((u8*)buf, byte_count);
    x_off = 0;
    src = (u16*)g_battle_menu_glyph_image;
    if (kind != 8) {
        if ((mode & 3) == 2) {
            x_off = words * 8;
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
            src_index = (i << 6) + 0xA;
        } else if (i >= height - 8) {
            src_index = (((i - height + 8) & 7) << 6) + 0x60A;
        } else {
            src_index = ((((i - 8) & 0xF) + 8) << 6) + 0xA;
        }
        row = buf + i * words + x_off;
        row[words - 1] = 0;
        row[words - 2] = 0;
        battle_menu_build_window_image_row(src, row, words, src_index);
    }
    if (kind != 8) {
        g_battle_menu_window_sprite_glyph.x = 0x58;
        g_battle_menu_window_sprite_glyph.y = 0;
        if ((mode & 0xC) == 4) {
            g_battle_menu_window_sprite_glyph.x = 0x68;
        }
        if ((mode & 3) == 1) {
            g_battle_menu_window_sprite_glyph.y = 0x10;
        }
        g_battle_menu_window_sprite_position.x = (width >> 1) + tail_offset - 8;
        if ((mode & 3) != 2) {
            g_battle_menu_window_sprite_position.y = 0x38;
        } else {
            g_battle_menu_window_sprite_position.y = 0;
        }
        g_battle_menu_window_sprite_position.stride = words * 4;
        i = g_battle_threads[g_battle_current_thread_id].task_words[5] & 0xF0;
        if (i != 0) {
            if ((mode & 3) == 1) {
                g_battle_menu_window_sprite_position.x -= 0x10;
            } else {
                g_battle_menu_window_sprite_position.x += 0x10;
            }
        }
        if (g_battle_menu_window_sprite_position.x < 0x10) {
            g_battle_menu_window_sprite_position.x = 0x10;
        }
        if (width - 0x10 < g_battle_menu_window_sprite_position.x) {
            g_battle_menu_window_sprite_position.x = width - 0x10;
        }
        blit_text_glyph((u16*)g_battle_menu_glyph_image, buf, &g_battle_menu_window_sprite_glyph,
            &g_battle_menu_window_sprite_position);
        if (i != 0) {
            battle_text_mirror_packed_nibble_region_horizontal(
                (u8*)buf, (world_gfx_4bpp_rect_t*)&g_battle_menu_window_sprite_position);
        }
    }
    if (keep != 0) {
        return buf;
    }
    LoadImage(rect, (u32*)buf);
    battle_thread_wait_frames(1);
    battle_menu_free_memory(buf);
}
