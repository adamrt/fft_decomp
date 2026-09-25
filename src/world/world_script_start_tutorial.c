#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

void world_script_start_tutorial(s32 index) {
    g_world_script_tutorial_id = index;
    if (index != 0) {
        world_gfx_load_tim_pair_by_index(index, &g_world_tutorial_saved_game_image);
        g_world_script_tutorial_command_ptr = g_world_tutorial_script_buffer;
        g_world_text_section_pointers[23] = g_world_tutorial_text_buffer;
        world_card_init_menu_state();
        world_card_build_save_image(-1);
        bcopy(g_world_load_work_buffer, &g_world_tutorial_saved_game_image, 0x1E00);
        main_file_load_checked_to_address(0x11A7F, 0x2000, g_world_load_work_buffer);
        g_world_load_work_buffer->options.show_unequippable_items = GAME_OPTION_ON;
        g_world_load_work_buffer->options.max_equip_at_job_change = g_main_game_options.fields.max_equip_at_job_change;
        g_world_load_work_buffer->options.sound_mode = g_main_game_options.fields.sound_mode;
        world_card_load_globals_from_save_image(1);
        g_world_script_tutorial_command_active = 0;
        world_text_generate_formation_unit_name_string();
    }
}
