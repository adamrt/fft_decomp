/* Initial inventory and roster used before the title name and birthday screens. */

#include "fft/encounter.h"
#include "fft/item.h"
#include "fft/party.h"
#include "fft/screen.h"
#include "fft/script_variable.h"
#include "fft/unit.h"

/* Empty the inventory, then stock the new-game consumables: five Potions, two each of Hi-Potion,
 * Eye Drop and Phoenix Down, and one of each other basic restorative. */
void item_new_game_inventory_init(void) {
    s32 remaining;
    u8* quantity;

    for (remaining = ITEM_ID_COUNT - 1, quantity = &g_item_quantities[ITEM_ID_COUNT - 1]; remaining >= 0;
        remaining--, quantity--) {
        *quantity = 0;
    }

    g_item_quantities[ITEM_ID_POTION] = 5;
    g_item_quantities[ITEM_ID_HI_POTION] = 2;
    g_item_quantities[ITEM_ID_X_POTION] = 1;
    g_item_quantities[ITEM_ID_ETHER] = 1;
    g_item_quantities[ITEM_ID_HI_ETHER] = 1;
    g_item_quantities[ITEM_ID_ELIXIR] = 1;
    g_item_quantities[ITEM_ID_ANTIDOTE] = 1;
    g_item_quantities[ITEM_ID_EYE_DROP] = 2;
    g_item_quantities[ITEM_ID_ECHO_GRASS] = 1;
    g_item_quantities[ITEM_ID_MAIDENS_KISS] = 1;
    g_item_quantities[ITEM_ID_SOFT] = 1;
    g_item_quantities[ITEM_ID_HOLY_WATER] = 1;
    g_item_quantities[ITEM_ID_REMEDY] = 1;
    g_item_quantities[ITEM_ID_PHOENIX_DOWN] = 2;
}

enum { STARTING_ENCOUNTER_ID = 254, STARTING_WAR_FUNDS = 2000 };

/* Start a new roster: empty it, then generate Ramza alone (mode 0) or load the
 * party from ENTD encounter 254 (mode 2); any other mode leaves it empty. The
 * inventory is reset to the new-game stock and the war funds to 2000. */
void party_new_game_init(int mode) {
    screen_world_text_init();
    party_clear();

    if (mode == PARTY_NEW_GAME_RAMZA) {
        party_unit_generate(UNIT_TYPE_RAMZA);
    } else if (mode == PARTY_NEW_GAME_ENCOUNTER) {
        encounter_party_init(STARTING_ENCOUNTER_ID);
    }

    item_new_game_inventory_init();
    script_variable_set(EVENT_SCRIPT_VAR_WAR_FUNDS, STARTING_WAR_FUNDS);
}
