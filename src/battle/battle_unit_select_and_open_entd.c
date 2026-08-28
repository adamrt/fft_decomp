#include "fft/battle.h"
#include "fft/script_variables.h"
#include "psx/types.h"

/* volatile views: the target reloads these globals at every use. */
extern volatile s32 g_battle_entd_selection_mode;
extern volatile s32 g_main_entd_current_event_id;

s32 battle_unit_select_and_open_entd(void) {
    volatile s32* current_event_id;
    s32 event_id;
    s32 selection_mode;

    event_id = battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_ENTD);
    selection_mode = g_battle_entd_selection_mode;
    current_event_id = &g_main_entd_current_event_id;
    *current_event_id = event_id;

    if (selection_mode == 1) {
        *current_event_id = 250;
    } else if (selection_mode == 2) {
        *current_event_id = (rand() * 5) / 0x8000 + 75;
    } else if (selection_mode == 3) {
        *current_event_id = (rand() * 59) / 0x8000 + 1;
    } else if ((selection_mode == 4) && (g_battle_map_id != 0)) {
        *current_event_id = g_battle_map_id;
    }

    return main_entd_open_file();
}
