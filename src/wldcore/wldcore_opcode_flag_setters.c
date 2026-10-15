#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_set_saved_bit(void) {
    wldcore_set_bit_value(g_main_secondary_saved_data_bits, (u8)(g_wldcore_active_saved_record.instruction >> 8), 1);
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

void wldcore_opcode_clear_saved_bit(void) {
    wldcore_set_bit_value(g_main_secondary_saved_data_bits, (u8)(g_wldcore_active_saved_record.instruction >> 8), 0);
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

void wldcore_opcode_set_local_flag(void) {
    wldcore_set_bit_value(g_wldcore_sound_novel_local_flags, (u8)(g_wldcore_active_saved_record.instruction >> 8), 1);
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

void wldcore_opcode_clear_local_flag(void) {
    wldcore_set_bit_value(g_wldcore_sound_novel_local_flags, (u8)(g_wldcore_active_saved_record.instruction >> 8), 0);
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}
