#include "fft/world.h"
#include "psx/types.h"

/* Scans a 0xFF-terminated byte list for value. */
s32 world_list_contains_value(s32 value) {
    s32 i;
    s32 g_main_item_move_find_flags = 0;

    for (i = 0; g_world_spell_quote_exception_skillsets[i] != 0xFF; i++) {
        if (g_world_spell_quote_exception_skillsets[i] == value) {
            g_main_item_move_find_flags = 1;
            break;
        }
    }
    return g_main_item_move_find_flags;
}
