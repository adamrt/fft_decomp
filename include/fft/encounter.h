#ifndef ENCOUNTER_H
#define ENCOUNTER_H

/* The current encounter: its ENTD identifier, the loaded file and the entry a unit is being built from,
 * and the MAIN routines that load the file and fill the party roster from it. */

#include "fft/unit_entd.h"
#include "psx/types.h"

/* g_encounter_selection_mode: which ENTD unit_entd_select_and_open opens. */
typedef enum unit_entd_selection {
    UNIT_ENTD_SELECTION_SCRIPT_VARIABLE = 0, /* EVENT_SCRIPT_VAR_CURRENT_ENTD */
    UNIT_ENTD_SELECTION_ENTD_250 = 1,
    UNIT_ENTD_SELECTION_RANDOM_75_TO_79 = 2,
    UNIT_ENTD_SELECTION_RANDOM_1_TO_59 = 3,
    UNIT_ENTD_SELECTION_MAP_ID = 4 /* the map's own id when it is not 0 */
} unit_entd_selection_e;

extern s32 g_encounter_current_id;
extern entd_file_t* g_encounter_set;
extern const entd_unit_t* g_encounter_current_unit;

/* unit_entd_selection_e. BATTLE's load ordering requires the qualified declaration;
 * a local volatile pointer access changes the original instructions. */
#ifdef FFT_BATTLE
extern volatile s32 g_encounter_selection_mode;
#else
extern s32 g_encounter_selection_mode;
#endif

int encounter_open_file(void);
void encounter_init_party(int encounter_id);

/* Current encounter, NULL while loading, or (entd_encounter_t *)-1 when
 * no encounter is selected. */
entd_encounter_t* encounter_get_current(void);

#endif
