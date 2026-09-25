#include "fft/battle.h"

void battle_menu_build_main(void) {
    world_menu_entry_t* parameter;
    battle_menu_record_t* record;
    s32 unit_id;

    parameter = (world_menu_entry_t*)battle_thread_get_current_parameter_1();
    unit_id = g_battle_active_turn_unit.battle_id;
    record = &g_battle_menu_unit_selection_records[unit_id];
    g_battle_thread_contexts[g_battle_current_thread_id].function_parameter_2 = 1; /* redraw */
    /* The target passes two arguments to the argument-less loop. */
    ((void (*)(s32, s32))battle_menu_run_icon_selection_loop)(1, unit_id);
    record->bytes[0] = *(u8*)&parameter->selected_index;
    battle_thread_exit_current();
}
