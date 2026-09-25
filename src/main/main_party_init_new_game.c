#include "fft/main.h"
#include "fft/world.h"

enum {
    NEW_GAME_GENERATE_DEFAULT_UNIT = 0,
    NEW_GAME_LOAD_EVENT_PARTY = 2,
    STARTING_EVENT_ID = 0xfe,
    STARTING_WAR_FUNDS = 2000,
};

void main_party_init_new_game(int mode) {
    world_menu_load_text_1bd8();
    main_party_clear_all();

    if (mode == NEW_GAME_GENERATE_DEFAULT_UNIT) {
        main_party_generate_unit(MAIN_UNIT_TYPE_RAMZA);
    } else if (mode == NEW_GAME_LOAD_EVENT_PARTY) {
        main_entd_init_event_unit_data(STARTING_EVENT_ID);
    }

    main_item_init_new_game_inventory();
    world_script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS, STARTING_WAR_FUNDS);
}
