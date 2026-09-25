#include "fft/wldcore.h"
#include "psx/types.h"

/* WLDCORE's entrypoint; the overlay is loaded at 0x80067000. */
s32 wldcore_entrypoint(void) {
    s32 buffer_index;

    for (;;) {
        if (wldcore_handle_exit_request() != 0) {
            return 0;
        }
        wldcore_init_subsystems();
        wldcore_fade_start_screen(0, 0x10);
        for (;;) {
            if ((g_main_system_flags & 1) == 0) {
                break;
            }
            main_file_poll_load(&g_main_file_cd_state);
            wldcore_sound_process_audio_queue();
            buffer_index = world_gs_get_active_buffer();
            g_active_graphics_buffer_index = buffer_index;
            world_gs_setworkbase(&g_wldcore_gfx_world_primitive_buffers + buffer_index * 0xE000);
            wldcore_fade_step_screen();
            wldcore_map_pulse_dot_colors();
            wldcore_menu_dispatch_idle_handler();
            wldcore_process_frame_gate();
            if (g_main_system_flags & 2) {
                wldcore_gfx_draw_world_frame_with_map();
            } else {
                wldcore_gfx_draw_world_frame();
            }
            if (g_main_system_flags & 0x40) {
                SetDispMask(0);
            } else {
                SetDispMask(1);
            }
            main_noop_800449ec();
        }
        if (g_main_system_flags & 0x10000) {
            continue;
        }
        wldcore_finalize_loop();
        {
            s32 flags = g_main_system_flags;

            if (flags & 0x10000000) {
                return 5;
            }
            if (flags & 0x02000000) {
                return 4;
            }
            if (flags & 0x200000) {
                return 2;
            }
            /* Returning through display_mask keeps two separate `return 2`
             * blocks; a plain `return 2` here cross-jumps into the one above. */
            {
                s32 display_mask;

                if (flags & 0x40000) {
                    display_mask = 2;
                } else {
                    display_mask = (flags & 0x8000) > 0;
                }
                return display_mask;
            }
        }
    }
}
