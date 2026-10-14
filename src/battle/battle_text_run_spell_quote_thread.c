#include "fft/battle.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/battle_text.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Task 0x46: show message 0xa800 | parameter 2 about the unit whose battle id
 * is parameter 1.
 *
 * The id also fills the three text substitution values. The text thread runs
 * with window mode 0x11, or 0x12 when the unit is on the upper half of the
 * screen, from SPELL.MES (companion data file 4) loaded into a 16 KiB
 * buffer. */
void battle_text_run_spell_quote_thread(void) {
    SVECTOR screen_coords;
    unit_stats_t* unit;
    s32 id; /* Battle id, then the unit's unit_id: one variable in the target. */
    s32 window_mode;
    s32 message_id;

    g_battle_system_function_thread_busy = 1;
    thread_set_current_task_id(THREAD_TASK_UNIT_MESSAGE);
    script_set_event_speed(2);
    thread_set_parameters(THREAD_SLOT_MENU, 0, 0, 1);
    thread_wait_until_inactive(THREAD_SLOT_MENU);
    thread_wait_frames(3);
    window_mode = 0x11;
    id = battle_thread_get_current_parameter_1();
    g_text_substitution_values[0] = id;
    g_text_substitution_values[1] = id;
    g_text_substitution_values[2] = id;
    unit = battle_unit_get_stats_from_battle_id(id);
    unit_project_to_screen(battle_unit_get_runtime_id_by_battle_id(id), &screen_coords);
    id = unit->unit_id;
    if (screen_coords.vy < 0x80) {
        window_mode = 0x12;
    }
    message_id = battle_thread_get_current_parameter_2() | 0xA800;
    g_menu_companion_data_file_buffer = main_heap_alloc(0x4000);
    menu_request_open_companion_data_file(BATTLE_COMPANION_FILE_SPELL_MES);
    g_text_section_pointers[TEXT_SECTION_UNIT_MESSAGE] = g_menu_companion_data_file_buffer;
    thread_start(THREAD_SLOT_MENU, battle_text_run_dialog_thread);
    thread_set_parameters(THREAD_SLOT_MENU, window_mode, message_id, id);
    thread_wait_until_inactive(THREAD_SLOT_MENU);
    main_heap_free(g_menu_companion_data_file_buffer);
    g_menu_pending_selection[0] = 7;
    script_set_event_speed(1);
    g_battle_system_function_thread_busy = 0;
    thread_exit_current();
}
