#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

s32 wldcore_entrypoint(void) {
    s32 buffer_index;

    for (;;) {
        if (wldcore_handle_exit_request() != 0) {
            return 0;
        }
        wldcore_init_subsystems();
        wldcore_fade_start_screen(0, 0x10);
        for (;;) {
            if ((g_main_system_flags & MAIN_SYSTEM_FLAG_RUNNING) == 0) {
                break;
            }
            main_file_poll_load(&g_main_file_cd_state);
            wldcore_sound_process_audio_queue();
            buffer_index = GsGetActiveBuff();
            g_active_graphics_buffer_index = buffer_index;
            GsSetWorkBase(g_wldcore_gfx_world_primitive_buffers + buffer_index * 0xE000);
            wldcore_fade_step_screen();
            wldcore_map_pulse_dot_colors();
            wldcore_menu_dispatch_idle_handler();
            wldcore_process_frame_gate();
            if (g_main_system_flags & MAIN_SYSTEM_FLAG_DRAW_MAP) {
                wldcore_gfx_draw_world_frame_with_map();
            } else {
                wldcore_gfx_draw_world_frame();
            }
            if (g_main_system_flags & MAIN_SYSTEM_FLAG_DISPLAY_OFF) {
                SetDispMask(0);
            } else {
                SetDispMask(1);
            }
            main_system_frame_stub();
        }
        if (g_main_system_flags & MAIN_SYSTEM_FLAG_RESTART) {
            continue;
        }
        wldcore_finalize_loop();
        {
            s32 flags = g_main_system_flags;

            if (flags & MAIN_SYSTEM_FLAG_RETURN_TO_TITLE) {
                return 5;
            }
            if (flags & MAIN_SYSTEM_FLAG_DEEP_DUNGEON_BATTLE) {
                return 4;
            }
            if (flags & MAIN_SYSTEM_FLAG_TUTORIAL_EVENT) {
                return 2;
            }
            /* Returning through a local keeps two separate `return 2`
             * blocks; a plain `return 2` here cross-jumps into the one above. */
            {
                s32 result;

                if (flags & MAIN_SYSTEM_FLAG_STORY_EVENT_REPLAY) {
                    result = 2;
                } else {
                    result = (flags & MAIN_SYSTEM_FLAG_RANDOM_BATTLE) > 0;
                }
                return result;
            }
        }
    }
}
