#ifndef STATUS_H
#define STATUS_H

/* Status-effect routines shared by the BATTLE and WORLD event systems. Status
 * IDs and stored status sets are defined in unit.h. */

#include "fft/unit.h"

/* event status changes */
void status_queue_current_status_graphics(s32 battle_id, s32 enabled);
void status_inflict_by_event_unit_id(s32 event_unit_id, s32 status, s32 inflict, s32 less_strict_removal);

#endif
