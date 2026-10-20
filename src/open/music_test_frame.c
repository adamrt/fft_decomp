/* Run WORLD's menu frame inside OPEN's title loop while the Music Test is open. */

#include "fft/gfx.h"
#include "fft/input.h"
#include "fft/menu_input.h"
#include "fft/screen.h"
#include "fft/system.h"
#include "fft/title.h"
#include "psx/types.h"

/* While SYSTEM_RUNTIME_FLAG_WORLD_MENU_FRAME is set (the music test), run
 * one WORLD menu frame with the blocked buttons withheld, drawing from entry
 * g_music_test_menu_otag_index of the ordering table OPEN is building. The
 * active buffer index is flipped for the duration of the call. */
void music_test_menu_frame_run(void) {
    if (g_system_runtime_flags & SYSTEM_RUNTIME_FLAG_WORLD_MENU_FRAME) {
        u32 blocked_buttons = g_menu_blocked_buttons;

        g_gfx_active_buffer_index ^= 1;
        screen_allowed_buttons_set(~blocked_buttons);
        screen_menu_frame_update(
            &g_gfx_otags[g_gfx_active_buffer_index ^ 1][g_music_test_menu_otag_index], g_input_current_buttons);
        g_gfx_active_buffer_index ^= 1;
    }
}
