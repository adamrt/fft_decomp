#include "fft/battle.h"
#include "fft/battle_text.h"
#include "fft/equip.h"
#include "psx/types.h"

void equip_text_start_help_thread(s32 thread_data) {
    if (g_equip_text_help_message_id > 0) {
        battle_text_save_pointer_table();
        battle_text_relocate_pointer_table((const u32*)&g_equip_help_text_table);
        battle_thread_start(1, battle_text_character_handling_thread);
        battle_thread_set_parameters(1, thread_data + 0x38, g_equip_text_help_message_id, 0);
        g_event_mode = 1;
    }
}
