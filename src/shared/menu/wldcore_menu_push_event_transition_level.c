#include "fft/wldcore.h"
#include "psx/libgpu.h"

/* Pushes menu level type 0x23 with the given mode. Mode 2 only starts a
 * screen fade; any other mode copies VRAM (0x100, 0) 0x80x0x100 to
 * (0x200, 0x100), clears (0x100, 0) 0x100x0xf0, sets up the indicator sprite
 * and picks two random values for the level. */
void wldcore_menu_push_event_transition_level(s32 mode) {
    RECT rect;
    s32 depth;

    wldcore_file_seek_to_battle_bin();
    if (mode == WLDCORE_EVENT_TRANSITION_MODE_FADE) {
        g_main_system_flags |= MAIN_SYSTEM_FLAG_FREEZE_ANIMATION;
        wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_OUT, 0x20);
    } else {
        rect.x = 0x100;
        rect.y = 0;
        rect.w = 0x80;
        rect.h = 0x100;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_DRAW_MAP | MAIN_SYSTEM_FLAG_HIDE_MAP_DOTS
            | MAIN_SYSTEM_FLAG_HIDE_MAP_ROUTES | MAIN_SYSTEM_FLAG_NO_BACKGROUND_IMAGE
            | MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE | MAIN_SYSTEM_FLAG_HIDE_CALENDAR_DATE
            | MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
        g_wldcore_map_projection_state.marker.render.flags |= WORLD_RENDER_RECORD_HIDDEN;
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
        wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 0x10);
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].event_transition.frame = 0;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].event_transition.anim_step = 0;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].event_transition.drift_direction
            = (rand() * 4) >> 15;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].event_transition.spin_direction
            = (rand() * 2) >> 15;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].event_transition.phase
            = WLDCORE_EVENT_TRANSITION_PHASE_SPIN_IN;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].event_transition.fade_phase
            = WLDCORE_EVENT_TRANSITION_FADE_NONE;
    }
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].event_transition.mode = mode;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_EVENT_TRANSITION;
    g_wldcore_menu_stack_depth = depth + 1;
}
