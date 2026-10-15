#include "fft/script.h"
#include "psx/types.h"

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * script_run_scenario_conditions and script_check_scenario_condition. Nothing
 * on the disc calls it or points at it; the name is a neutral placeholder
 * until a reference turns up. */
void script_noop_9(void) {
}
