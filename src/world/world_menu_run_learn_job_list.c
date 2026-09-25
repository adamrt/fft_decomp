#include "fft/world.h"
#include "psx/pad.h"

/* Runs the learn menu's job list for the selected formation unit.
 *
 * The first call builds the unlocked-job list and installs the job
 * level/JP menu-script callbacks. Returns -1 when cancelled, 1 when a job
 * with learnable abilities is confirmed (caching its id and generic job
 * index), and 0 while the list is still open. */
s32 world_menu_run_learn_job_list(void) {
    s16 x;
    s16 y;

    if (g_world_learn_job_list_initialized == 0) {
        world_script_set_vsync_mode_and_event_speed(2);
        world_job_build_unit_list(g_world_formation_selected_unit_index, g_world_menu_learn_unlocked_jobs, 0);
        g_world_menu_script_callbacks[0] = world_menu_get_learn_job_level;
        g_world_menu_script_callbacks[2] = (s32 (*)(s32))world_menu_get_learn_job_jp;
        g_world_menu_script_callbacks[5] = world_job_refresh_next_level_jp_requirement;
        g_world_menu_script_callbacks[6] = (s32 (*)(s32))world_menu_refresh_learn_job_total_jp;
        g_world_menu_script_callbacks[13] = world_menu_refresh_learn_skillset_empty;
        world_gfx_get_sprite_slot(9, &x, &y, g_world_menu_learn_unlocked_jobs);
        world_menu_init_scrollable_list(g_world_menu_learn_unlocked_jobs, x, y, (u32)g_world_text_job_names);
        g_world_learn_job_list_initialized = 1;
    }
    g_world_menu_description_text_id = g_world_menu_learn_unlocked_jobs[g_world_menu_cursor_position] + 0x6000;
    if (g_world_input_primary_repeat & PSX_PAD_CROSS) {
        world_script_set_vsync_mode_and_event_speed(0);
        g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
        g_world_learn_job_list_initialized = 0;
        return -1;
    }
    if (g_world_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (world_menu_refresh_learn_skillset_empty(g_world_menu_cursor_position) == 0
            || g_world_formation_current_menu == 0xD) {
            g_world_learn_selected_job = g_world_menu_learn_unlocked_jobs[g_world_menu_cursor_position];
            /* The definition's u16 parameter and s16 return conversions would change this call's codegen. */
            g_world_learn_selected_job_index = ((s16 (*)(s16))world_job_get_generic_index)(g_world_learn_selected_job);
            world_menu_get_learn_job_level(g_world_menu_cursor_position);
            world_menu_get_learn_job_jp(g_world_menu_cursor_position);
            world_menu_refresh_learn_job_total_jp(g_world_menu_cursor_position);
            world_job_refresh_next_level_jp_requirement(g_world_menu_cursor_position);
            world_gfx_set_sprite_slot(
                9, g_world_menu_cursor_position, g_world_menu_scroll_offset, (u16*)g_world_menu_learn_unlocked_jobs);
            g_world_learn_job_list_initialized = 0;
            g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
            return 1;
        }
        world_text_show_message_and_play_sound(0xC007, 0x30);
    }
    world_menu_run_script_with_palette_mode(
        g_world_learn_job_list_script, g_world_input_primary_repeat, g_world_thread_task_active);
    return 0;
}
