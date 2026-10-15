#include "fft/wldcore.h"
#include "psx/libetc.h"

/* Per-frame WLDCORE render pass: clears the current ordering table, runs the
 * menu frame and marker/display-object passes, draws screen fades, waits for
 * VSync and, unless system flag 0x200 is set, uploads the scratch image to the
 * other display buffer before drawing the ordering table. */
void wldcore_gfx_draw_world_frame(void) {
    RECT rect;

    GsClearOt(0, 0, &g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    DrawSync(0);
    wldcore_draw_close_indicator_if_pending(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_menu_run_world_frame(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_switch_to_stack(getScratchAddr(255));
    wldcore_gfx_draw_context_value_display(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_gfx_draw_calendar_date(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    wldcore_proposition_submit_marker_request(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
    window_dispatch_record_list_variant(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index],
        g_wldcore_window_render_object_queue, g_wldcore_window_render_object_count);
    wldcore_fade_draw_screen_overlays();
    wldcore_restore_previous_stack();
    DrawSync(0);
    wldcore_reset_game_if_special_keycode_is_pressed();
    VSync(0);
    GsSwapDispBuff();
    if (!(g_main_system_flags & MAIN_SYSTEM_FLAG_NO_BACKGROUND_IMAGE)) {
        wldcore_gfx_set_display_rect(g_active_graphics_buffer_index == 0, &rect);
        LoadImage(&rect, g_wldcore_scratch_buffer);
    }
    GsDrawOt(&g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index]);
}
