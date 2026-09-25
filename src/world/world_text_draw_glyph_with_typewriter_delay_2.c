#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

typedef struct world_text_glyph_blit_desc {
    s16 x; /* 0x00 */
    s16 y; /* 0x02 */
    u8 _unused_04[4];
    s32 width; /* 0x08 */
} world_text_glyph_blit_desc_t;

extern world_text_glyph_blit_desc_t g_world_text_message_box_state;

/* Draw the pending glyph into VRAM and hold it for the typewriter delay.
 *
 * Cousin of world_text_draw_glyph_with_typewriter_delay without the request pointer, task-id checks or
 * script counters. The ring byte is read before the phase and the inner bound
 * is `(s32)width / 2`, which keeps the per-row recomputation of the target.
 */
void world_text_draw_glyph_with_typewriter_delay_2(s32 x, s32 y, s32 wait) {
    s32 slot;
    s32 i;
    s32 j;
    s32 phase;
    u8* dst;
    u8* src;
    u32 width;
    s32 ring;
    s32 offset;

    if (g_world_text_message_box_glyph.mode != 0x10 && g_world_text_message_box_glyph.mode != 0x20
        && g_world_text_message_box_glyph.mode != 0) {
        world_thread_yield();
        return;
    }
    for (;;) {
        slot = 0x10;
        for (i = 0; i < 3; i++) {
            if (g_world_thread_current_id == g_world_text_message_box_slot_threads[i]) {
                slot = i;
                g_world_text_message_box_slot_threads[i] = g_world_thread_current_id;
                break;
            }
        }
        if (i == 3) {
            for (j = 0; j < 3; j++) {
                if (g_world_text_message_box_slot_threads[j] == 0) {
                    slot = j;
                    g_world_text_message_box_slot_threads[j] = g_world_thread_current_id;
                    g_world_text_message_box_slot_carry_offset[slot] = 0;
                    g_world_text_message_box_slot_prev_width[slot] = 0;
                    break;
                }
            }
        }
        if (slot != 0x10) {
            break;
        }
        world_thread_yield();
    }

    ring = (u8)g_world_text_message_box_slot_glyph_counters[slot];
    phase = g_world_text_message_box_glyph.pixel_x & 3;
    dst = g_world_text_message_box_glyph_images[slot][ring];
    src = g_world_text_message_box_previous_images[slot];
    if (g_world_text_glyph_widths[g_world_text_message_box_glyph.code] == 4
        || g_world_text_message_box_glyph.code == TEXT_SPACE) {
        if (phase == 2) {
            width = 8;
            g_world_text_message_box_vram_rect.w = 2;
        } else {
            width = 4;
            g_world_text_message_box_vram_rect.w = 1;
        }
    } else if (g_world_text_glyph_widths[g_world_text_message_box_glyph.code] == 2) {
        width = 4;
        g_world_text_message_box_vram_rect.w = 1;
    } else if (g_world_text_glyph_widths[g_world_text_message_box_glyph.code] == 6) {
        width = 8;
        g_world_text_message_box_vram_rect.w = 2;
    } else {
        width = 12;
        g_world_text_message_box_vram_rect.w = 3;
    }

    if (phase == 2) {
        for (i = 0; i < 14; i++) {
            g_world_text_message_box_column_scratch[i]
                = *(src + (s32)(i * g_world_text_message_box_slot_prev_width[slot]) / 2
                    + g_world_text_message_box_slot_carry_offset[slot]);
        }
        for (i = 0; i < 14; i++) {
            dst[(s32)(i * width) / 2] = g_world_text_message_box_column_scratch[i];
        }
    }

    if (g_world_text_glyph_widths[g_world_text_message_box_glyph.code] == 4
        || g_world_text_message_box_glyph.code == TEXT_SPACE) {
        if (phase == 2) {
            g_world_text_message_box_slot_carry_offset[slot] = 2;
        } else {
            g_world_text_message_box_slot_carry_offset[slot] = 1;
        }
    } else if (g_world_text_glyph_widths[g_world_text_message_box_glyph.code] == 2) {
        if (phase == 2) {
            g_world_text_message_box_slot_carry_offset[slot] = 1;
        } else {
            g_world_text_message_box_slot_carry_offset[slot] = 0;
        }
    } else if (g_world_text_glyph_widths[g_world_text_message_box_glyph.code] == 6) {
        if (phase == 2) {
            g_world_text_message_box_slot_carry_offset[slot] = 3;
        } else {
            g_world_text_message_box_slot_carry_offset[slot] = 2;
        }
    } else {
        if (phase == 2) {
            g_world_text_message_box_slot_carry_offset[slot] = 5;
        } else {
            g_world_text_message_box_slot_carry_offset[slot] = 4;
        }
    }
    g_world_text_message_box_slot_prev_width[slot] = width;

    for (j = 0; j < 14; j++) {
        offset = j * width;
        for (i = phase >> 1; i < (s32)width / 2; i++) {
            if (g_world_text_message_box_glyph.mode == 0) {
                (dst + i)[offset / 2] = 0;
            } else {
                (dst + i)[offset / 2] = ((u8*)g_world_menu_glyph_sheet
                    + ((((g_world_text_message_box_glyph.pixel_x + (i << 1)) & 0xF) + 8) / 2))[(j << 7) + 0x400];
            }
        }
    }

    if (g_world_text_message_box_glyph.code != TEXT_SPACE) {
        g_world_text_message_box_state.x = phase;
        g_world_text_message_box_state.y = 0;
        g_world_text_message_box_state.width = width;
        world_text_blit_font_glyph_to_4bpp(g_world_text_glyph_bitmap_data + g_world_text_message_box_glyph.code * 35,
            dst, (u16*)&g_world_text_message_box_state, g_world_text_message_box_glyph.color);
        g_world_text_message_box_vram_rect.x = (x >> 2) + 0x1c0;
        g_world_text_message_box_vram_rect.y = y;
        g_world_text_message_box_vram_rect.h = 14;
        LoadImage(&g_world_text_message_box_vram_rect, (u32*)dst);
    }
    g_world_text_message_box_slot_glyph_counters[slot]++;
    world_script_copy_bytes(src, dst, 84);
    if (wait == 0) {
        DrawSync(0);
        g_world_text_message_box_slot_glyph_counters[slot] = 0;
    } else if (wait >= 2) {
        for (i = 0; i < wait * (3 - g_world_event_speed); i++) {
            world_thread_wait_frames(1);
        }
        g_world_text_message_box_slot_glyph_counters[slot] = 0;
    } else {
        if (PadRead(1) & 0x20) {
            g_world_text_message_box_glyph_delay = 8;
        } else if ((g_main_game_options.value & 0x7000) == 0) {
            g_world_text_message_box_glyph_delay = 8;
        } else if ((g_main_game_options.value & 0x7000) == 0x1000) {
            g_world_text_message_box_glyph_delay = 2;
        } else {
            g_world_text_message_box_glyph_delay = 1;
        }
        if (g_world_text_message_box_slot_glyph_counters[slot] >= g_world_text_message_box_glyph_delay) {
            for (i = 0; i < 3 - g_world_event_speed; i++) {
                world_thread_wait_frames(1);
            }
            g_world_text_message_box_slot_glyph_counters[slot] = 0;
        }
    }
    if (g_world_text_message_box_glyph.mode != 0) {
        g_world_sound_effect_id_to_play = MAIN_SFX_TEXT_GLYPH;
    }
}
