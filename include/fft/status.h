#ifndef STATUS_H
#define STATUS_H

/* Status-effect routines shared by the BATTLE and WORLD event systems. Status
 * IDs and stored status sets are defined in unit.h. */

#include "psx/types.h"

/* event status changes */

/* shared between programs */
extern s32 g_status_display_redraw_request;

/* shared with the per-program event interpreter and dialog thread */
void status_queue_current_status_graphics(s32 battle_id, s32 enabled);

#endif
