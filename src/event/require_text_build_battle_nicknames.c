#include "fft/battle.h"
#include "fft/battle_text.h"
#include "fft/unit_slots.h"
#include "psx/types.h"

void require_text_build_battle_nicknames(void) {
    u8* output;
    s32 unit_id;

    output = g_battle_text_section_pointers[8];
    unit_id = 0;
    do {
        battle_stats_t* unit = battle_unit_get_stats_from_battle_id(unit_id);
        s32 length = 0;

        do {
            u8 value = unit->name[length];
            *output = value;
            if ((value & 0xfe) != 0xfe) {
                output++;
                if (length == 15) {
                    *output++ = 0xfe;
                }
            } else {
                output++;
                break;
            }
            length++;
        } while (length < 16);
        unit_id++;
    } while (unit_id < BATTLE_UNIT_SLOT_COUNT);
}
