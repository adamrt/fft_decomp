#include "fft/battle_text.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/menu_types.h"
#include "fft/options.h"
#include "fft/script_variables.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Draw the pending glyph of a message into VRAM and hold it for the
 * typewriter delay.
 *
 * One of three slots is reserved for the calling thread, keyed on
 * g_world_thread_current_id; a thread that finds none yields and retries. The
 * slot owns a ring of eight 0x54-byte images and the previous image, so a
 * glyph whose pen position lands mid-byte can re-emit the tail its
 * predecessor already drew. `delay` selects the pacing: 0 returns after a
 * DrawSync, 1 waits once every g_world_text_glyphs_per_wait glyphs (8 while button 0x20 is held,
 * otherwise from the message-speed option), and larger values wait `delay`
 * times the event-speed frame count after every glyph.
 *
 * The ring byte is read before the phase so the pen-x load lands in its own
 * register behind the slot arithmetic, and the inner bound is `(s32)pitch / 2`
 * so it is recomputed per row, as in the target.
 */
void world_text_draw_glyph_with_typewriter_delay(s32 pen_x, s32 pen_y, s32 delay) {
    s32 slot;
    s32 i;
    s32 j;
    s32 phase;
    s32 glyph;
    s32 kind;
    s32 next_glyph;
    s32 next_kind;
    s32 ring;
    u32 pitch;
    u8* image;
    u8* previous;
    s32 row;
    s32 offset;
    world_text_glyph_request_t* request;

    request = &g_world_text_typewriter_glyph;
    if (g_world_text_typewriter_glyph.bits_per_pixel == 0x10 || g_world_text_typewriter_glyph.bits_per_pixel == 0) {
        for (;;) {
            slot = 16;
            for (i = 0; i < 3; i++) {
                if (g_world_thread_current_id == g_world_menu_slot_owner_thread_ids[i]) {
                    slot = i;
                    g_world_menu_slot_owner_thread_ids[i] = g_world_thread_current_id;
                    break;
                }
            }
            if (i == 3) {
                for (row = 0; row < 3; row++) {
                    if (g_world_menu_slot_owner_thread_ids[row] == 0) {
                        slot = row;
                        g_world_menu_slot_owner_thread_ids[row] = g_world_thread_current_id;
                        g_world_text_slot_source_columns[row] = 0;
                        g_world_text_slot_row_pitches[row] = 0;
                        break;
                    }
                }
            }
            if (slot != 16) {
                break;
            }
            world_thread_yield();
        }

        ring = (u8)g_world_menu_slot_states[slot];
        phase = request->x & 3;
        image = g_world_text_typewriter_glyph_images[slot][ring];
        previous = g_world_text_typewriter_previous_images[slot];
        glyph = request->glyph;
        kind = g_world_text_glyph_widths[glyph];
        if (kind == 4 || glyph == TEXT_SPACE) {
            if (phase == 2) {
                pitch = 8;
                g_world_text_typewriter_vram_rect.w = 2;
            } else {
                pitch = 4;
                g_world_text_typewriter_vram_rect.w = 1;
            }
        } else if (kind == 2) {
            pitch = 4;
            g_world_text_typewriter_vram_rect.w = 1;
        } else if (kind == 6) {
            pitch = 8;
            g_world_text_typewriter_vram_rect.w = 2;
        } else {
            pitch = 12;
            g_world_text_typewriter_vram_rect.w = 3;
        }

        if (phase == 2) {
            offset = 0;
            for (i = 0; i < 14; i++) {
                j = offset / 2;
                offset += g_world_text_slot_row_pitches[slot];
                g_world_text_typewriter_column_scratch[i] = (previous + j)[g_world_text_slot_source_columns[slot]];
            }
            for (i = 0; i < 14; i++) {
                image[(s32)(i * pitch) / 2] = g_world_text_typewriter_column_scratch[i];
            }
        }

        next_glyph = request->glyph;
        next_kind = g_world_text_glyph_widths[next_glyph];
        if (next_kind == 4 || next_glyph == 0xFA) {
            g_world_text_slot_source_columns[slot] = phase == 2 ? 2 : 1;
        } else if (next_kind == 2) {
            if (phase == 2) {
                g_world_text_slot_source_columns[slot] = 1;
            } else {
                g_world_text_slot_source_columns[slot] = 0;
            }
        } else if (next_kind == 6) {
            g_world_text_slot_source_columns[slot] = phase == 2 ? 3 : 2;
        } else {
            g_world_text_slot_source_columns[slot] = phase == 2 ? 5 : 4;
        }
        g_world_text_slot_row_pitches[slot] = pitch;

        for (row = 0; row < 14; row++) {
            offset = row * pitch;
            for (i = phase >> 1; i < (s32)pitch / 2; i++) {
                if (request->bits_per_pixel == 0) {
                    (image + i)[offset / 2] = 0;
                } else {
                    (image + i)[offset / 2] = ((u8*)g_world_menu_glyph_sheet
                        + ((((request->x + (i << 1)) & 0xF) + 8) / 2))[(row << 7) + 0x400];
                }
            }
        }

        if (request->glyph != TEXT_SPACE) {
            g_world_text_typewriter_state.origin_x = phase;
            g_world_text_typewriter_state.origin_y = 0;
            g_world_text_typewriter_state.stride = pitch;
            world_script_set_variable(EVENT_SCRIPT_VAR_PRINTED_CHARACTER_COUNT,
                world_script_get_variable(EVENT_SCRIPT_VAR_PRINTED_CHARACTER_COUNT) + 1);
            world_text_blit_font_glyph_to_4bpp(request->glyph * 0x23 + g_world_text_glyph_bitmap_data, image,
                (u16*)&g_world_text_typewriter_state.origin_x, request->palette);
            g_world_text_typewriter_vram_rect.x
                = (pen_x >> 2) + (s16)(world_script_get_variable(EVENT_SCRIPT_VAR_TYPEWRITER_VRAM_X_OFFSET) + 0x1C0);
            g_world_text_typewriter_vram_rect.y = pen_y;
            g_world_text_typewriter_vram_rect.h = 14;
            LoadImage(&g_world_text_typewriter_vram_rect, (u32*)image);
        }

        g_world_menu_slot_states[slot] = g_world_menu_slot_states[slot] + 1;
        world_script_copy_bytes(previous, image, 0x54);

        if (delay == 0) {
            DrawSync(0);
            g_world_menu_slot_states[slot] = 0;
        } else if (delay >= 2) {
            for (i = 0; i < delay * (3 - g_world_event_speed); i++) {
                if (world_thread_get_current_task_id() == NATIVE_THREAD_TASK_STOP_REQUEST) {
                    break;
                }
                world_thread_wait_frames(1);
            }
            g_world_menu_slot_states[slot] = 0;
        } else {
            if (PadRead(1) & 0x20) {
                g_world_text_glyphs_per_wait = 8;
            } else if ((g_main_game_options.value & 0x7000) == 0) {
                g_world_text_glyphs_per_wait = 8;
            } else if ((g_main_game_options.value & 0x7000) == 0x1000) {
                g_world_text_glyphs_per_wait = 2;
            } else {
                g_world_text_glyphs_per_wait = 1;
            }
            if (g_world_menu_slot_states[slot] >= g_world_text_glyphs_per_wait) {
                for (i = 0; i < 3 - g_world_event_speed; i++) {
                    if (world_thread_get_current_task_id() == NATIVE_THREAD_TASK_STOP_REQUEST) {
                        break;
                    }
                    world_thread_wait_frames(1);
                }
                g_world_menu_slot_states[slot] = 0;
            }
        }

        if (world_script_get_variable(EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE) == 0 && request->bits_per_pixel != 0) {
            g_world_sound_effect_id_to_play = MAIN_SFX_TEXT_GLYPH;
        }
    }
}
