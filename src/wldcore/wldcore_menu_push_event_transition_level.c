#include "fft/wldcore.h"
#include "psx/gpu.h"

/* Pushes menu level type 0x23 with the given mode. Mode 2 only starts a
 * screen fade; any other mode copies VRAM (0x100, 0) 0x80x0x100 to
 * (0x200, 0x100), clears (0x100, 0) 0x100x0xf0, sets up the indicator sprite
 * and picks two random values for the level. */
void wldcore_menu_push_event_transition_level(s32 mode) {
    RECT rect;
    s32 depth;

    wldcore_build_global_file_header();
    if (mode == 2) {
        g_main_system_flags |= 0x08000000;
        wldcore_fade_start_screen(2, 0x20);
    } else {
        rect.x = 0x100;
        rect.y = 0;
        rect.w = 0x80;
        rect.h = 0x100;
        g_main_system_flags |= 0x3A32;
        g_wldcore_map_projection_state.marker.flags |= 0x10;
        MoveImage(&rect, 0x200, 0x100);
        DrawSync(0);
        rect.x = 0x100;
        rect.y = 0;
        rect.w = 0x100;
        rect.h = 0xF0;
        ClearImage(&rect, 0, 0, 0);
        g_wldcore_menu_indicator_sprite.attribute = 0x42000000;
        g_wldcore_menu_indicator_sprite.x = 0x20;
        g_wldcore_menu_indicator_sprite.y = 0x20;
        g_wldcore_menu_indicator_sprite.tpage = GetTPage(2, 0, 0x100, 0);
        g_wldcore_menu_indicator_sprite.w = 0xFF;
        g_wldcore_menu_indicator_sprite.mx = 0xA0;
        g_wldcore_menu_indicator_sprite.my = 0x98;
        g_wldcore_menu_indicator_sprite.scaley = ONE;
        g_wldcore_menu_indicator_sprite.scalex = ONE;
        g_wldcore_menu_indicator_sprite.cy = 0;
        g_wldcore_menu_indicator_sprite.cx = 0;
        g_wldcore_menu_indicator_sprite.v = 0;
        g_wldcore_menu_indicator_sprite.u = 0;
        g_wldcore_menu_indicator_sprite.h = 0xF0;
        g_wldcore_menu_indicator_sprite.rotate = 0;
        g_wldcore_menu_indicator_sprite.r = 0x80;
        g_wldcore_menu_indicator_sprite.g = 0x80;
        g_wldcore_menu_indicator_sprite.b = 0x80;
        wldcore_sound_enqueue_audio_command(2, 0x10);
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].event_transition.frame = 0;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].event_transition.anim_step = 0;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].event_transition.drift_direction
            = (rand() * 4) >> 15;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].event_transition.spin_direction
            = (rand() * 2) >> 15;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].event_transition.phase = 0;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].event_transition.fade_phase = 0;
    }
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].event_transition.mode = mode;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_EVENT_TRANSITION;
    g_wldcore_menu_stack_depth = depth + 1;
}
