#include "fft/wldcore.h"

/* World Map Script 0x1C - SetVar: read next two script args and set a variable. */
void wldcore_opcode_map_set_var(void) {
    s32 variable_id;
    s32 value;

    wldcore_script_read_operand_pair(&variable_id, &value);
    world_script_set_variable(variable_id, value);
}
