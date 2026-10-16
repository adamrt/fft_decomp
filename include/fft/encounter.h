#ifndef ENCOUNTER_H
#define ENCOUNTER_H

/* The encounter (ENTD) data that sets up a battle's units. */

#include "fft/unit_entd.h"
#include "psx/types.h"

extern s32 g_encounter_current_event_id;
extern entd_file_t* g_encounter_set;
extern entd_unit_t* g_encounter_current_unit;
extern s32 g_encounter_selection_mode; /* unit_entd_selection_e */
int encounter_open_file(void);
void encounter_init_event_units(int event_id);

/* Current encounter, NULL while loading, or (entd_encounter_t *)-1 when
 * no encounter is selected. */
entd_encounter_t* encounter_get_current(void);

#endif
