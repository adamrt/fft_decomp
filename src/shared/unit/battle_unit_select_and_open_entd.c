#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/script.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Dropping either volatile view moves the mode load past the initial ENTD store. */
extern volatile s32 g_battle_entd_selection_mode;

/* Chooses the encounter's ENTD id and starts reading its ENTD file.
 *
 * The id is script variable EVENT_SCRIPT_VAR_CURRENT_ENTD unless
 * g_battle_entd_selection_mode overrides it: 250, a random id 75..79, a random id
 * 1..59, or the map id when that is not 0. Returns main_entd_open_file's result. */
s32 battle_unit_select_and_open_entd(void) {
    volatile s32* current_event_id;
    s32 event_id;
    s32 selection_mode;

    event_id = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_ENTD);
    selection_mode = g_battle_entd_selection_mode;
    current_event_id = &g_main_entd_current_event_id;
    *current_event_id = event_id;

    if (selection_mode == BATTLE_ENTD_SELECTION_ENTD_250) {
        *current_event_id = 250;
    } else if (selection_mode == BATTLE_ENTD_SELECTION_RANDOM_75_TO_79) {
        *current_event_id = (rand() * 5) / 0x8000 + 75;
    } else if (selection_mode == BATTLE_ENTD_SELECTION_RANDOM_1_TO_59) {
        *current_event_id = (rand() * 59) / 0x8000 + 1;
    } else if ((selection_mode == BATTLE_ENTD_SELECTION_MAP_ID) && (g_map_id != 0)) {
        *current_event_id = g_map_id;
    }

    return main_entd_open_file();
}
