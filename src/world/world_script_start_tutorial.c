#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

/*
 * Start tutorial `index` (0 only clears the tutorial id).
 *
 * Loads the tutorial's files and binds its command stream and messages, then
 * swaps the game state for the tutorial's: the current game is serialized
 * into g_world_tutorial_state.saved_game, the 0x2000-byte tutorial save image
 * at sector 0x11A7F is loaded in its place (keeping the player's job-change
 * equip and sound options, with unequippable items shown) and unpacked into
 * the globals.
 */
void world_script_start_tutorial(s32 index) {
    g_world_script_tutorial_id = index;
    if (index != 0) {
        world_script_load_tutorial_files(index, &g_world_tutorial_state);
        g_world_script_tutorial_command_ptr = g_world_tutorial_state.script;
        g_text_section_pointers[WORLD_TEXT_SECTION_TUTORIAL_MESSAGES] = g_world_tutorial_state.messages;
        world_card_init_menu_state();
        world_card_build_save_image(-1);
        bcopy(g_world_load_work_buffer, &g_world_tutorial_state.saved_game, sizeof(g_world_tutorial_state.saved_game));
        main_file_load_checked_to_address(DISC_SECTOR_MENU_FFTSAVE_DAT, 0x2000, g_world_load_work_buffer);
        g_world_load_work_buffer->options.show_unequippable_items = GAME_OPTION_ON;
        g_world_load_work_buffer->options.max_equip_at_job_change = g_main_game_options.fields.max_equip_at_job_change;
        g_world_load_work_buffer->options.sound_mode = g_main_game_options.fields.sound_mode;
        world_card_load_globals_from_save_image(1);
        g_world_script_tutorial_command_active = 0;
        world_text_generate_formation_unit_name_string();
    }
}
