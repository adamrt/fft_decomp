#include "fft/world.h"
#include "psx/types.h"

void world_script_advance_date_by_one_year(void) {
    world_script_set_variable(EVENT_SCRIPT_VAR_DATE_ADVANCE, 0x16D);
}
