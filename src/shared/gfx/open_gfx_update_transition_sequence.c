#include "fft/open.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Advance the new-game prologue by one frame: the picture crossfade and the text
 * of the current transition entry. Returns 1 while it runs and 0 when it has
 * finished.
 *
 * An entry is one page of text lines. NEXT_ENTRY loads the next entry, or
 * finishes at target_index: its line count, texture page and image operation,
 * where 0x10 + n prepares the crossfade to picture n and 0x20 waits for that
 * picture's read and starts the crossfade. NEXT_STEP starts the next line,
 * reading every line's texture row and width from the entry on the first; the
 * timer starts at -16 and the line is revealed one pixel per frame. After the
 * last line the page waits for Circle (WAIT_CIRCLE), dims from 0x80 by 2 per
 * frame (DIM) and the next entry follows.
 *
 * Start sets SKIP: the lines keep advancing while the brightness runs down, and
 * the sequence ends once it is 0 and the crossfade has stopped. */
s32 open_gfx_update_transition_sequence(void) {
    u16 flags;
    s32 i;
    open_gfx_transition_entry_t* entry;
    s32 image_command;

    open_gfx_step_image_crossfade();
    if (!(g_open_gfx_transition.flags & OPEN_GFX_TRANSITION_SKIP)) {
        if (g_open_input_new_button_presses & PSX_PAD_START) {
            main_sound_play_sfx(MAIN_SFX_COMPLETE);
            g_open_gfx_transition.flags |= OPEN_GFX_TRANSITION_SKIP;
        }
        flags = g_open_gfx_transition.flags;
        if (!(flags & OPEN_GFX_TRANSITION_SKIP)) {
            if (flags & OPEN_GFX_TRANSITION_DIM) {
                g_open_gfx_transition.intensity -= 2;
                open_gfx_draw_transition_line_strips(g_open_gfx_transition.intensity);
                if (g_open_gfx_transition.intensity == 0) {
                    g_open_gfx_transition.intensity = 0x80;
                    g_open_gfx_transition.flags
                        = (g_open_gfx_transition.flags ^ OPEN_GFX_TRANSITION_DIM) | OPEN_GFX_TRANSITION_NEXT_ENTRY;
                }
                return 1;
            }
            if (flags & OPEN_GFX_TRANSITION_WAIT_CIRCLE) {
                if (g_open_input_new_button_presses & PSX_PAD_CIRCLE) {
                    g_open_gfx_transition.flags = (flags ^ OPEN_GFX_TRANSITION_WAIT_CIRCLE) | OPEN_GFX_TRANSITION_DIM;
                    g_open_gfx_transition.intensity = 0x80;
                }
                open_gfx_draw_transition_line_strips(g_open_gfx_transition.intensity);
                return 1;
            }
            if (flags & OPEN_GFX_TRANSITION_NEXT_ENTRY) {
                flags ^= OPEN_GFX_TRANSITION_NEXT_ENTRY;
                g_open_gfx_transition.flags = flags;
                if (g_open_gfx_transition.current_index == g_open_gfx_transition.target_index) {
                    return 0;
                }
                g_open_gfx_transition.flags = flags | OPEN_GFX_TRANSITION_NEXT_STEP;
                entry = g_open_gfx_transition.entry_table[g_open_gfx_transition.current_index];
                g_open_gfx_transition.entry = entry;
                g_open_gfx_transition.step_count = (u8)entry->header;
                g_open_gfx_transition.tpage_x = (entry->header & 0xf00) >> 2;
                g_open_gfx_transition.tpage_y = (entry->header >> 4) & 0x100;
                /* The high nibble selects the operation; the low nibble selects its image. */
                image_command = (u8)(entry->header >> 16);
                switch (image_command & OPEN_GFX_TRANSITION_IMAGE_OPERATION_MASK) {
                case OPEN_GFX_TRANSITION_PREPARE_IMAGE:
                    open_gfx_copy_file_image_to_210x180_buffer(image_command & OPEN_GFX_TRANSITION_IMAGE_INDEX_MASK);
                    break;
                case OPEN_GFX_TRANSITION_START_CROSSFADE:
                    file_wait_for_pending_variant();
                    g_open_gfx_image_copy_row_flags[0] = 1;
                    break;
                }
                {
                    s16* index = &g_open_gfx_transition.current_index;

                    g_open_gfx_transition.step = 0;
                    (*index)++;
                }
            }
        }
    }

    flags = g_open_gfx_transition.flags;
    if (flags & OPEN_GFX_TRANSITION_NEXT_STEP) {
        g_open_gfx_transition.flags = flags ^ OPEN_GFX_TRANSITION_NEXT_STEP;
        /* Every line is out: the page waits for Circle. */
        if (g_open_gfx_transition.step == g_open_gfx_transition.step_count) {
            g_open_gfx_transition.flags = (flags ^ OPEN_GFX_TRANSITION_NEXT_STEP)
                | (OPEN_GFX_TRANSITION_NEXT_ENTRY | OPEN_GFX_TRANSITION_WAIT_CIRCLE);
            open_gfx_draw_transition_line_strips(g_open_gfx_transition.intensity);
            return 1;
        }
        /* Byte indexing preserves the retail address-add order and entry reloads. */
        if (g_open_gfx_transition.step == 0) {
            for (i = 0; i < g_open_gfx_transition.step_count; i++) {
                g_open_gfx_transition.line_widths[i]
                    = g_open_gfx_transition.entry->line_data[i * sizeof(open_gfx_transition_line_t)
                        + offsetof(open_gfx_transition_line_t, width)];
                g_open_gfx_transition.line_texture_rows[i]
                    = g_open_gfx_transition.entry->line_data[i * sizeof(open_gfx_transition_line_t)
                        + offsetof(open_gfx_transition_line_t, texture_row)];
            }
        }
        {
            s16* step = &g_open_gfx_transition.step;

            g_open_gfx_transition.timer = -0x10;
            (*step)++;
        }
    }
    if (++g_open_gfx_transition.timer == g_open_gfx_transition.line_widths[g_open_gfx_transition.step - 1]) {
        g_open_gfx_transition.flags |= OPEN_GFX_TRANSITION_NEXT_STEP;
    }
    if (g_open_gfx_transition.flags & OPEN_GFX_TRANSITION_SKIP) {
        if (g_open_gfx_transition.intensity != 0) {
            g_open_gfx_transition.intensity -= 2;
        } else if (g_open_gfx_image_copy_row_flags[0] == 0) {
            return 0;
        }
    }
    open_gfx_draw_transition_line_strips(g_open_gfx_transition.intensity);
    return 1;
}
