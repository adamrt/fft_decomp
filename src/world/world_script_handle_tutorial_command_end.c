#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Tutorial "end" command: fade the tutorial overlay out, then release the
 * saved screen state and restore the pre-tutorial world buffers. */
s32 world_script_handle_tutorial_command_end(void) {
    if (g_world_script_tutorial_command_active != 0) {
        if (world_gfx_update_fade_out_tile() == 0) {
            g_world_script_tutorial_id = 0;
            main_heap_call_free(g_world_tutorial_text_buffer);
            main_heap_call_free(g_world_tutorial_script_buffer);
            bcopy(&g_world_tutorial_saved_game_image, g_world_load_work_buffer, 0x1E00);
            world_card_load_globals_from_save_image(1);
            world_text_generate_formation_unit_name_string();
            main_heap_call_free(g_world_load_work_buffer);
            world_formation_build_record_list(0, (world_formation_unit_t**)&g_world_formation_unit_pointers, 0);
            return 0;
        }
    } else {
        world_gfx_start_increasing_fade();
        g_world_script_tutorial_command_active = 1;
    }
}
