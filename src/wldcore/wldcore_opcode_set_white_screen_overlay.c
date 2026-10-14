#include "fft/wldcore.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Opcode handler 42 of the table at 0x8009ecfc, immediately after
 * wldcore_opcode_fade_screen_in. Drains the GPU and waits a frame, then either
 * arms the full-screen white overlay (sortbox color 0xff/0xff/0xff, state bit
 * 3 set, transfer bits cleared, state flag 0x100 set, menus blacked out) or
 * tears it down again (color cleared, state bits 0/1/3 cleared, flag 0x100
 * cleared, menus back to neutral 0x80 brightness). The operand byte selects:
 * zero arms it, non-zero clears it. boxes[0].r/green/blue are the cataloged
 * fields at g_wldcore_screen_fade_state+0x24..0x26, gating the draw in
 * wldcore_fade_draw_screen_overlays.
 *
 * The world_menu_set_brightness call and the 0xc store are written once per
 * arm; jump.c cross-jumps the common tail from the g_wldcore_active_saved_record.state_flags store
 * onward, leaving only the three argument constants in the arms.
 */
void wldcore_opcode_set_white_screen_overlay(void) {
    u16* flags;

    DrawSync(0);
    VSync(0);
    if ((u8)(g_wldcore_active_saved_record.instruction >> 8) == 0) {
        u32* state = &g_wldcore_screen_fade_state.flags[0];

        g_wldcore_screen_fade_state.boxes[0].r = 0xff;
        g_wldcore_screen_fade_state.boxes[0].g = 0xff;
        g_wldcore_screen_fade_state.boxes[0].b = 0xff;
        g_main_system_flags &= ~MAIN_SYSTEM_FLAG_FADING;
        *state = (*state & ~(WLDCORE_SCREEN_FADE_RUNNING | WLDCORE_SCREEN_FADE_OUT)) | WLDCORE_SCREEN_FADE_BOX_VISIBLE;
        g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_WHITE_OVERLAY;
        world_menu_set_brightness(0, 0, 0);
    } else {
        u32* state = &g_wldcore_screen_fade_state.flags[0];

        g_wldcore_screen_fade_state.boxes[0].r = 0;
        g_wldcore_screen_fade_state.boxes[0].g = 0;
        g_wldcore_screen_fade_state.boxes[0].b = 0;
        g_main_system_flags &= ~MAIN_SYSTEM_FLAG_FADING;
        *state &= ~(WLDCORE_SCREEN_FADE_RUNNING | WLDCORE_SCREEN_FADE_OUT | WLDCORE_SCREEN_FADE_BOX_VISIBLE);
        g_wldcore_active_saved_record.state_flags &= ~WLDCORE_OPCODE_STATE_WHITE_OVERLAY;
        world_menu_set_brightness(0x80, 0x80, 0x80);
    }
    flags = &g_wldcore_active_saved_record.state_flags;
    *flags = *flags | (WLDCORE_OPCODE_STATE_FETCH | WLDCORE_OPCODE_STATE_YIELD);
}
