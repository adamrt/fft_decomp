#include "fft/battle.h"
#include "psx/types.h"

void battle_action_clear_knockback_flag(void) {
    u8* flag = &g_current_ability.knockback_flags;
    u16* target_data;
    if ((*flag & 0x80) != 0) {
        target_data = (u16*)g_battle_action_target_data;
        *flag = 0;
        target_data[8] = (u16)(target_data[8] & 0xBFFF);
    }
}
