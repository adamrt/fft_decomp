#include "fft/battle.h"
#include "fft/main.h"

s32 main_unit_init(entd_encounter_t* entd, s32 unit_id, s32 guest_id, s32 initialize_for_battle) {
    entd_unit_t* entd_unit = &entd->units[unit_id];

    g_current_entd_unit = entd_unit;
    if (entd_unit->sprite_set == CHARACTER_IDENTITY_ENTD_NONE) {
        return -2;
    }
    {
        battle_stats_t unit;
        battle_stats_t* unit_data = &unit;

        unit_data->misc_unit_id = 0xff;
        unit_data->entd_slot = 0x20;
        unit_data->existence = BATTLE_UNIT_EXISTENCE_ACTIVE;
        unit_data->formation_index = BATTLE_FORMATION_INDEX_NONE;
        main_unit_init_for_battle(unit_data, entd_unit, initialize_for_battle, 0);
        return main_party_save_unit(unit_data, guest_id);
    }
}
