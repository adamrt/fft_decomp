#ifndef UNIT_EDITOR_H
#define UNIT_EDITOR_H

/* DEBUGCHR's developer unit editor and the event-library thread that opens it. */

#include "psx/types.h"

extern s32 g_debug_unit_editor_active;

/* Run DEBUGCHR's developer unit editor; the normal rider/mount Status entrance is separate. */
void debugchr_editor_entrypoint(void);

/* The event library's menu thread: closes the unit view panels, loads DEBUGCHR and runs debugchr_editor_entrypoint. */
void debug_unit_editor_open(void);

#endif
