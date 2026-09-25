#include "fft/battle.h"

s32* battle_script_get_variable_word_pointer_from_id(s32 id) {
    s32* result;
    s32 off;

    if (id < 0x80) {
        result = (s32*)(id * 4 + (u32)g_battle_script_variables);
    } else if (id < 0x360) {
        off = ((id - 0x80) / 32) * 4 + 0x200;
        result = (s32*)(off + (u32)g_battle_script_variables);
    } else if (id < 0x400) {
        off = ((id - 0x360) / 8) * 4 + 0x25C;
        result = (s32*)(off + (u32)g_battle_script_variables);
    } else {
        battle_thread_exit_current();
    }
    return result;
}
