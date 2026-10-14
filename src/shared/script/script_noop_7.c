#include "fft/script.h"
#include "psx/types.h"

/* Empty in BATTLE and WORLD; in BATTLE it sits between script_noop_6 and
 * script_assign_bitset_flag. Nothing on the disc calls it or points at it; the
 * name is a neutral placeholder until a reference turns up. */
void script_noop_7(void) {
}
