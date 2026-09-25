#include "fft/battle.h"
#include "psx/types.h"

void battle_text_build_unit_name_list(void) {
    u8* output;
    s32 unit_id;
    battle_stats_t* unit;
    s32 length;

    battle_text_init_menu_section_pointers();
    unit_id = 0;
    output = g_battle_text_section_pointers[8];
    do {
        unit = battle_unit_get_stats_from_battle_id(unit_id);
        if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
            *output++ = 0xfe;
        } else {
            length = 0;
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
        }
        unit_id++;
    } while (unit_id < BATTLE_UNIT_SLOT_COUNT);
}
