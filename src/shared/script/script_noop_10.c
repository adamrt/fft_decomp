#include "fft/script.h"
#include "psx/types.h"

/* Empty in BATTLE and WORLD; in BATTLE it sits between script_return_zero_1
 * and script_frame_after_requests_stub. Nothing on the disc calls it or points
 * at it; the name is a neutral placeholder until a reference turns up. */
void script_noop_10(void) {
}
