#include "fft/battle.h"
#include "fft/battle_text.h"
#include "fft/world.h"
#include "psx/types.h"

void battle_text_restore_pointer_table(void) {
    battle_copy_bytes(g_battle_text_section_pointers, g_battle_text_pointer_backup, 0x80);
    g_battle_text_pointer_backup_active = 0;
}
