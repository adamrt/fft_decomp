#include "fft/battle.h"
#include "fft/battle_text.h"
#include "fft/main_heap.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Task 0x46: show message 0xa800 | parameter 2 about the unit whose battle id
 * is parameter 1.
 *
 * The id also fills the three text substitution values. The text thread runs
 * with window mode 0x11, or 0x12 when the unit is on the upper half of the
 * screen, from the secondary companion executable loaded into a 16 KiB
 * buffer. */
void battle_text_run_unit_message_thread(void) {
    s16 screen_coords[2];
    battle_stats_t* unit;
    s32 id; /* Battle id, then the unit's unit_id: one variable in the target. */
    s32 window_mode;
    s32 message_id;

    g_battle_system_function_thread_busy = 1;
    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_UNIT_MESSAGE);
    battle_script_set_event_speed(2);
    battle_thread_set_parameters(8, 0, 0, 1);
    battle_thread_wait_until_inactive(8);
    battle_thread_wait_frames(3);
    window_mode = 0x11;
    id = battle_thread_get_current_parameter_1();
    g_battle_text_substitution_values[0] = id;
    g_battle_text_substitution_values[1] = id;
    g_battle_text_substitution_values[2] = id;
    unit = battle_unit_get_stats_from_battle_id(id);
    battle_unit_project_misc_to_screen(battle_unit_get_misc_id_by_battle_id(id), screen_coords);
    id = unit->unit_id;
    if (screen_coords[1] < 0x80) {
        window_mode = 0x12;
    }
    message_id = battle_thread_get_current_parameter_2() | 0xA800;
    g_battle_text_message_buffer = game_malloc(0x4000);
    battle_menu_request_open_secondary_companion_executable(4);
    g_battle_text_section_pointers[21] = g_battle_text_message_buffer;
    battle_thread_start(8, battle_text_character_handling_thread);
    battle_thread_set_parameters(8, window_mode, message_id, id);
    battle_thread_wait_until_inactive(8);
    main_heap_free(g_battle_text_message_buffer);
    g_battle_menu_pending_selection[0] = 7;
    battle_script_set_event_speed(1);
    g_battle_system_function_thread_busy = 0;
    battle_thread_exit_current();
}
