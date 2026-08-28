#include "fft/main_runtime.h"
#include "fft/open.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/types.h"

void wldcore_menu_run_world_frame(GsOT* context) {
    u32* otag_entries;
    u32* sprite_entries;
    u32* menu_entries;

    if (g_main_system_flags & 0x80) {
        return;
    }
    world_formation_set_ordering_table_pointer(context->org + 15);
    world_formation_set_menu_entry_mask(~g_wldcore_menu_result);
    if (g_main_system_flags & 0x20000) {
        otag_entries = context->org;
        g_world_gfx_active_otag_entries = otag_entries + g_wldcore_menu_ordering_table_offset;
        world_menu_draw_active_window_frames();
        sprite_entries = context->org;
        /* The definition takes an s16 frame argument; the target passes the full
         * product without the truncation that prototype would add. */
        ((void (*)(u32*, u32, s32))world_script_update_event_frame_input)(
            sprite_entries + g_wldcore_menu_ordering_table_offset + 2, g_wldcore_current_button_input,
            g_active_graphics_buffer_index * 240);
        if (g_world_sound_effect_id_to_play != -1) {
            wldcore_sound_play_effect(g_world_sound_effect_id_to_play);
        }
    } else {
        menu_entries = context->org;
        world_formation_run_menu_frame(
            menu_entries + g_wldcore_menu_ordering_table_offset, g_wldcore_current_button_input);
    }
}
