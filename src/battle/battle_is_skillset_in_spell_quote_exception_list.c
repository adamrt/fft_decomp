#include "fft/battle.h"
#include "psx/types.h"

/* Test membership in the 0xff-terminated skillset exception list. */
s32 battle_is_skillset_in_spell_quote_exception_list(s32 skillset_id) {
    s32 i;
    s32 found = 0;

    for (i = 0; g_battle_spell_quote_exception_skillsets[i] != 0xff; i++) {
        if (g_battle_spell_quote_exception_skillsets[i] == skillset_id) {
            found = 1;
            break;
        }
    }
    return found;
}
