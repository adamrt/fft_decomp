#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_load_formation_unit_name_and_face(void) {
    s32 value;

    world_menu_load_text_1bd8_with_face();
    world_text_generate_formation_unit_name_string();
    value = world_script_get_variable(EVENT_SCRIPT_VAR_DATE_ADVANCE);
    if (value != 0) {
        wldcore_advance_calendar_days(value);
        world_script_set_variable(EVENT_SCRIPT_VAR_DATE_ADVANCE, 0);
    }
}
