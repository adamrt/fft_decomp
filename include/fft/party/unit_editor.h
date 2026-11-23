#ifndef UNIT_EDITOR_H
#define UNIT_EDITOR_H

/* DEBUGCHR's developer unit editor. */

#include "psx/types.h"

extern s32 g_debug_unit_editor_active;

/* Run DEBUGCHR's developer unit editor; the normal rider/mount Status entrance is separate. */
void debugchr_editor_entrypoint(void);

#endif
