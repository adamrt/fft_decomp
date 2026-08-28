#include "fft/main_runtime.h"
#include "fft/wldcore.h"

/* Provisional: list-window state passed to 0x8008211c. Offsets are
 * established only by that routine's reads. */
typedef struct wldcore_list_window_state {
    s32 main_window;    /* 0x00 */
    s32 side_window;    /* 0x04 */
    s32 frame_render;   /* 0x08 */
    s32 content_render; /* 0x0c */
    u8 unknown_10[8];
    s32 mode;           /* 0x18 */
    s32 selected_entry; /* 0x1c */
    s32 upper_window;   /* 0x20 */
    s32 lower_window;   /* 0x24 */
    u8 unknown_28[0xc];
    s32 entry_kinds[1]; /* 0x34 */
} wldcore_list_window_state_t;

/* Set the window palettes of a list window: main window 10 (with sequence
 * set and frame_index/anim_counter cleared), side window 6, the frame render
 * record 2 and both auxiliary windows 5. Mode 2 also sets the content render
 * palette to 2; otherwise a selected entry of kind other than 2 hides the content
 * render record (flag 0x10) and sets system flag 0x800.
 *
 * The chained store keeps one main-window index for both cleared fields. */
void wldcore_window_set_list_window_states(wldcore_list_window_state_t* state) {
    g_wldcore_window_records[state->main_window].palette = 10;
    g_wldcore_window_records[state->side_window].palette = 6;
    g_wldcore_window_render_records[state->frame_render].palette = 2;
    g_wldcore_window_records[state->upper_window].palette = 5;
    g_wldcore_window_records[state->lower_window].palette = 5;
    g_wldcore_window_records[state->main_window].sequence = 1;
    g_wldcore_window_records[state->main_window].frame_index = g_wldcore_window_records[state->main_window].anim_counter
        = 0;
    if (state->mode == 2) {
        g_wldcore_window_render_records[state->content_render].palette = 2;
        return;
    }
    if (state->entry_kinds[state->selected_entry] != 2) {
        g_main_system_flags |= 0x800;
        g_wldcore_window_render_records[state->content_render].flags |= 0x10;
    }
}
