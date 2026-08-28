#include "fft/wldcore.h"
#include "fft/world.h"

void wldcore_opcode_set_menu_brightness(void) {
    char unused[16];

    world_menu_set_brightness(g_wldcore_active_saved_record.instruction.bytes.operand_0,
        g_wldcore_active_saved_record.instruction.bytes.operand_1,
        g_wldcore_active_saved_record.instruction.bytes.operand_2);
    g_wldcore_active_saved_record.state_flags |= 4;
}
