#include "fft/open.h"
#include "psx/pad.h"
#include "psx/types.h"

s32 open_gfx_update_transition_sequence(void) {
    u16 flags;
    s32 i;
    open_gfx_transition_entry_t* entry;
    s32 type;

    open_gfx_step_image_crossfade();
    if (!(g_open_gfx_transition.flags & 0x20)) {
        if (g_open_input_new_button_presses & PSX_PAD_START) {
            main_sound_play_sfx(0x85);
            g_open_gfx_transition.flags |= 0x20;
        }
        flags = g_open_gfx_transition.flags;
        if (!(flags & 0x20)) {
            if (flags & 0x10) {
                g_open_gfx_transition.intensity -= 2;
                open_gfx_draw_transition_line_strips(g_open_gfx_transition.intensity);
                if (g_open_gfx_transition.intensity == 0) {
                    g_open_gfx_transition.intensity = 0x80;
                    g_open_gfx_transition.flags = (g_open_gfx_transition.flags ^ 0x10) | 1;
                }
                return 1;
            }
            if (flags & 8) {
                if (g_open_input_new_button_presses & PSX_PAD_CIRCLE) {
                    g_open_gfx_transition.flags = (flags ^ 8) | 0x10;
                    g_open_gfx_transition.intensity = 0x80;
                }
                open_gfx_draw_transition_line_strips(g_open_gfx_transition.intensity);
                return 1;
            }
            if (flags & 1) {
                flags ^= 1;
                g_open_gfx_transition.flags = flags;
                if (g_open_gfx_transition.current_index == g_open_gfx_transition.target_index) {
                    return 0;
                }
                g_open_gfx_transition.flags = flags | 2;
                entry = g_open_gfx_transition.entry_table[g_open_gfx_transition.current_index];
                g_open_gfx_transition.entry = entry;
                g_open_gfx_transition.step_count = entry->bytes[0];
                g_open_gfx_transition.tpage_x = (entry->header & 0xf00) >> 2;
                g_open_gfx_transition.tpage_y = (entry->header >> 4) & 0x100;
                type = entry->bytes[2];
                switch (type & 0xf0) {
                case 0x10:
                    open_gfx_copy_file_image_to_210x180_buffer(type & 0xf);
                    break;
                case 0x20:
                    open_file_wait_for_pending();
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
    if (flags & 2) {
        g_open_gfx_transition.flags = flags ^ 2;
        if (g_open_gfx_transition.step == g_open_gfx_transition.step_count) {
            g_open_gfx_transition.flags = (flags ^ 2) | 9;
            open_gfx_draw_transition_line_strips(g_open_gfx_transition.intensity);
            return 1;
        }
        if (g_open_gfx_transition.step == 0) {
            for (i = 0; i < g_open_gfx_transition.step_count; i++) {
                g_open_gfx_transition.step_durations[i] = g_open_gfx_transition.entry->bytes[i * 4 + 5];
                g_open_gfx_transition.step_values[i] = g_open_gfx_transition.entry->bytes[i * 4 + 4];
            }
        }
        {
            s16* step = &g_open_gfx_transition.step;

            g_open_gfx_transition.timer = -0x10;
            (*step)++;
        }
    }
    if (++g_open_gfx_transition.timer == g_open_gfx_transition.step_durations[g_open_gfx_transition.step - 1]) {
        g_open_gfx_transition.flags |= 2;
    }
    if (g_open_gfx_transition.flags & 0x20) {
        if (g_open_gfx_transition.intensity != 0) {
            g_open_gfx_transition.intensity -= 2;
        } else if (g_open_gfx_image_copy_row_flags[0] == 0) {
            return 0;
        }
    }
    open_gfx_draw_transition_line_strips(g_open_gfx_transition.intensity);
    return 1;
}
