#include "fft/main.h"
#include "fft/unit.h"

/* Build a battle unit from one ENTD unit slot on the stack and save it into the
 * party roster. Returns -2 for an empty ENTD slot, otherwise
 * main_party_save_unit's result (0, or -1 when the roster is full). guest_id is
 * passed on as that function's allow_guest flag. */
s32 main_unit_init(entd_encounter_t* entd, s32 unit_id, s32 guest_id, s32 initialize_for_battle) {
    entd_unit_t* entd_unit = &entd->units[unit_id];

    g_main_entd_current_unit = entd_unit;
    if (entd_unit->sprite_set == CHARACTER_IDENTITY_ENTD_NONE) {
        return -2;
    }
    {
        unit_stats_t unit;
        unit_stats_t* unit_data = &unit;

        unit_data->battle_id = 0xff;
        unit_data->entd_slot = 0x20;
        unit_data->existence = BATTLE_UNIT_EXISTENCE_ACTIVE;
        unit_data->formation_index = BATTLE_FORMATION_INDEX_NONE;
        main_unit_init_for_battle(unit_data, entd_unit, initialize_for_battle, 0);
        return main_party_save_unit(unit_data, guest_id);
    }
}
