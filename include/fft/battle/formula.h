#ifndef FORMULA_H
#define FORMULA_H

/* The ability formulas' random rolls: live random numbers while an action executes, the fixed midpoint
 * for previews and AI simulations. */

#include "psx/types.h"

/* rand() while the action executes; the midpoint 0x4000 for previews and AI simulations. */
s32 formula_get_random(void);
s32 formula_roll_random_at_least(s32 limit, s32 threshold);

#endif
