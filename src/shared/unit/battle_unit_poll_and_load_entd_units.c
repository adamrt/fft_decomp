#include "fft/battle_unit.h"
#include "psx/types.h"

/* Loads the encounter's ENTD units once the ENTD file has arrived, then frees the
 * file. Returns 1 while the file is still loading, else 0 (also when there is no
 * current encounter). */
s32 battle_unit_poll_and_load_entd_units(void) {
    entd_encounter_t* entd_data;

    entd_data = main_entd_get_encounter();
    if (entd_data != (entd_encounter_t*)-1) {
        if (entd_data == 0) {
            return 1;
        }
        battle_unit_load_entd_units(entd_data, 0);
        main_heap_call_free(g_main_entd_set);
    }
    return 0;
}
