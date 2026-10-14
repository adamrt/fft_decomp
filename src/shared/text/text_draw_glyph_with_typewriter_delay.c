#include "fft/battle_menu.h"
#include "fft/battle_text.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/pad.h"

void text_draw_glyph_with_typewriter_delay(s32 pen_x, s32 pen_y, s32 delay) {
    s32 slot;
    s32 i;
    s32 j;
    s32 phase;
    u8 ring;
    s32 glyph;
    s32 kind;
    s32 next_glyph;
    s32 next_kind;
    s32 pitch;
    u8* image;
    u8* previous;
    s32 row;
    s32 offset;
    text_glyph_state_t* request;

    request = &g_text_typewriter_glyph;
    if (g_text_typewriter_glyph.mode == DIALOG_BOX_TYPE_PORTRAIT || g_text_typewriter_glyph.mode == 0) {
        for (;;) {
            slot = 16;
            for (i = 0; i < 3; i++) {
                if (g_thread_current_id == g_text_slot_owner_thread_ids[i]) {
                    slot = i;
                    g_text_slot_owner_thread_ids[i] = g_thread_current_id;
                    break;
                }
            }
            if (i == 3) {
                for (row = 0; row < 3; row++) {
                    if (g_text_slot_owner_thread_ids[row] == 0) {
                        slot = row;
                        g_text_slot_owner_thread_ids[row] = g_thread_current_id;
                        g_text_slot_source_columns[slot] = 0;
                        g_text_slot_row_pitches[slot] = 0;
                        break;
                    }
                }
            }
            if (slot != 16) {
                break;
            }
            thread_yield();
        }

        ring = g_text_slot_glyph_counts[slot];
        phase = request->x & 3;
        image = g_text_typewriter_glyph_images[slot][ring];
        previous = g_text_typewriter_previous_images[slot];
        glyph = request->glyph;
        kind = g_text_glyph_widths[glyph];
        if (kind == 4 || glyph == TEXT_SPACE) {
            if (phase == 2) {
                pitch = 8;
                g_text_typewriter_vram_rect.w = 2;
            } else {
                pitch = 4;
                g_text_typewriter_vram_rect.w = 1;
            }
        } else if (kind == 2) {
            pitch = 4;
            g_text_typewriter_vram_rect.w = 1;
        } else if (kind == 6) {
            pitch = 8;
            g_text_typewriter_vram_rect.w = 2;
        } else {
            pitch = 12;
            g_text_typewriter_vram_rect.w = 3;
        }

        if (phase == 2) {
            offset = 0;
            for (i = 0; i < 14; i++) {
                j = offset / 2;
                offset += g_text_slot_row_pitches[slot];
                g_text_typewriter_column_scratch[i] = (previous + j)[g_text_slot_source_columns[slot]];
            }
            for (i = 0; i < 14; i++) {
                image[i * pitch / 2] = g_text_typewriter_column_scratch[i];
            }
        }

        next_glyph = request->glyph;
        next_kind = g_text_glyph_widths[next_glyph];
        if (next_kind == 4 || next_glyph == TEXT_SPACE) {
            g_text_slot_source_columns[slot] = phase == 2 ? 2 : 1;
        } else if (next_kind == 2) {
            if (phase == 2) {
                g_text_slot_source_columns[slot] = 1;
            } else {
                g_text_slot_source_columns[slot] = 0;
            }
        } else if (next_kind == 6) {
            g_text_slot_source_columns[slot] = phase == 2 ? 3 : 2;
        } else {
            g_text_slot_source_columns[slot] = phase == 2 ? 5 : 4;
        }
        g_text_slot_row_pitches[slot] = pitch;

        for (row = 0; row < 14; row++) {
            offset = row * pitch;
            for (i = phase >> 1; i < pitch / 2; i++) {
                if (request->mode == 0) {
                    (image + i)[offset / 2] = 0;
                } else {
                    (image + i)[offset / 2] = (g_menu_glyph_sheet->pixels
                        + ((((request->x + (i << 1)) & 0xF) + 8) / 2))[(row << 7) + 0x400];
                }
            }
        }

        if (request->glyph != TEXT_SPACE) {
            g_text_typewriter_state.pen.rect.x = phase;
            g_text_typewriter_state.pen.rect.y = 0;
            g_text_typewriter_state.pen.stride = pitch;
            script_set_variable(EVENT_SCRIPT_VAR_PRINTED_CHARACTER_COUNT,
                script_get_variable(EVENT_SCRIPT_VAR_PRINTED_CHARACTER_COUNT) + 1);
            text_render_glyph_to_4bpp_image(g_text_glyph_bitmap_data + request->glyph * TEXT_GLYPH_BYTES, image,
                &g_text_typewriter_state.pen, request->palette);
            g_text_typewriter_vram_rect.x
                = (pen_x >> 2) + (s16)(script_get_variable(EVENT_SCRIPT_VAR_TYPEWRITER_VRAM_X_OFFSET) + 0x1C0);
            g_text_typewriter_vram_rect.y = pen_y;
            g_text_typewriter_vram_rect.h = 14;
            LoadImage(&g_text_typewriter_vram_rect, (u32*)image);
        }

        g_text_slot_glyph_counts[slot] = g_text_slot_glyph_counts[slot] + 1;
        mem_copy_bytes_nonempty(previous, image, 0x54);

        if (delay == 0) {
            DrawSync(0);
            g_text_slot_glyph_counts[slot] = 0;
        } else if (delay >= 2) {
            for (i = 0; i < delay * (3 - g_script_event_speed); i++) {
                if (thread_get_current_task_id() == THREAD_TASK_STOP_REQUEST) {
                    break;
                }
                thread_wait_frames(1);
            }
            g_text_slot_glyph_counts[slot] = 0;
        } else {
            if (PadRead(1) & PSX_PAD_CIRCLE) {
                g_text_glyphs_per_wait = 8;
            } else if (g_main_game_options.fields.message_display_speed == GAME_MESSAGE_DISPLAY_SPEED_FAST) {
                g_text_glyphs_per_wait = 8;
            } else if (g_main_game_options.fields.message_display_speed == GAME_MESSAGE_DISPLAY_SPEED_REGULAR) {
                g_text_glyphs_per_wait = 2;
            } else {
                g_text_glyphs_per_wait = 1;
            }
            if (g_text_slot_glyph_counts[slot] >= g_text_glyphs_per_wait) {
                for (i = 0; i < 3 - g_script_event_speed; i++) {
                    if (thread_get_current_task_id() == THREAD_TASK_STOP_REQUEST) {
                        break;
                    }
                    thread_wait_frames(1);
                }
                g_text_slot_glyph_counts[slot] = 0;
            }
        }

        if (script_get_variable(EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE) == 0 && request->mode != 0) {
            g_sound_pending_sfx_id = MAIN_SFX_TEXT_GLYPH;
        }
    }
}
