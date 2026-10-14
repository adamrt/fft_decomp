#include "fft/battle_text.h"
#include "psx/types.h"

/* Test membership in the 0xff-terminated skillset exception list. */
s32 text_is_spell_quote_exception_skillset(s32 skillset_id) {
    s32 i;
    s32 found = 0;

    for (i = 0; g_text_spell_quote_exception_skillsets[i] != SKILLSET_ID_NONE; i++) {
        if (g_text_spell_quote_exception_skillsets[i] == skillset_id) {
            found = 1;
            break;
        }
    }
    return found;
}
