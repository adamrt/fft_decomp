#include "fft/battle_text.h"
#include "fft/equip.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

typedef struct equip_thread_task {
    /* 0x00 */ void (*func)(void);
    /* 0x04 */ u16 unknown_04;
    /* 0x06 */ u16 text_thread_id; /* world_menu_text_binding_t.text_thread_id */
} equip_thread_task_t;

typedef struct equip_event_thread {
    /* 0x00 */ u8 unknown_00[0x1C];
    /* 0x1C */ u16 text_id; /* menu struct text entry */
    /* 0x1E */ u8 unknown_1e[0xE];
    /* 0x2C */ u16 header_id; /* menu struct header */
    /* 0x2E */ u8 unknown_2e[2];
    /* 0x30 */ equip_thread_task_t* task;
} equip_event_thread_t;

void equip_menu_open_submenu_thread(void) {
    equip_event_thread_t* thread;
    equip_thread_task_t* task;
    u32* input;
    s32 unknown_1c;
    s32 unknown_2c;
    u16 child_id;
    u16 task_arg;

    thread = (equip_event_thread_t*)g_battle_threads[g_battle_current_thread_id].function_parameter_1;
    thread->task->func();

    input = battle_script_get_controller_input_pointer(0);
    unknown_1c = thread->text_id;
    task = thread->task;
    unknown_2c = thread->header_id;
    g_equip_input_controller = input;
    child_id = task->text_thread_id;
    task_arg = task->unknown_04;
    *input = PSX_PAD_CIRCLE;
    battle_menu_handle_action(thread, 0);

    battle_thread_start(child_id, battle_text_character_handling_thread);
    battle_thread_set_parameters_4(child_id, unknown_2c, unknown_1c, task_arg, task_arg);
    battle_thread_wait_until_inactive(g_battle_current_thread_id - 1);
    battle_thread_wait_until_inactive(child_id);
    battle_thread_exit_current();
}
