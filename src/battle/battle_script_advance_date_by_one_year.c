#include "fft/battle.h"
#include "psx/types.h"

void battle_script_advance_date_by_one_year(void) {
    battle_script_set_variable(EVENT_SCRIPT_VAR_DATE_ADVANCE, 365);
}
