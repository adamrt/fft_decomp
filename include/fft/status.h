#ifndef STATUS_H
#define STATUS_H

/* Status-effect routines shared by the BATTLE and WORLD event systems. Status
 * IDs and stored status sets are defined in unit.h. */

#include "fft/unit.h"

/* event status changes */

/* shared between programs */
extern s32 g_status_display_redraw_request;

#endif
