#include "fft/battle.h"
#include "psx/types.h"

/* MoveConfirmMenu game-state handler. Command 7 keeps the move and finishes it.
 * Command 8 or 0xff undoes it and returns to choosing a destination, except after
 * a teleport, which is kept like command 7. */
void battle_state_handle_move_confirm_menu_state(void) {
    ai_command_t* selected_command_address;
    s32 selected_command;
    battle_unit_t* source_unit;
    battle_unit_t* casting_unit;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_main_input_current_buttons);
    selected_command_address = menu_get_selected_command_address();
    source_unit = battle_unit_get_source();
    casting_unit = battle_unit_get_casting();
    selected_command = selected_command_address->kind;
    switch (selected_command) {
    case 8:
    case 0xff:
        if (casting_unit->path.flags & BATTLE_EFFECTIVE_MOVEMENT_TELEPORT) {
        case 7:
            battle_unit_copy_rider_data_to_mount(source_unit, casting_unit);
            return;
        }
        move_undo_unit_move(casting_unit);
        battle_gfx_update_unit_palettes();
        battle_menu_close_move_help();
        return;
    }
}
