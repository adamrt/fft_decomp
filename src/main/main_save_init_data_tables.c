#include "fft/main.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Reset the save record fields and new-game data tables.
 *
 * Assembled with ASPSX < 2.30: the slot loop keeps five 4-instruction
 * `lui $at / addiu $at / addu $at,idx / sh 0($at)` forms, so this function
 * carries the per-function gcc-2.6.3_O2_aspsx-2.21 profile. */
void main_save_init_data_tables(void) {
    s32 i;
    wldcore_saved_record_t* record;

    for (i = 0; i < 5; i++) {
        record = &g_main_saved_records[i];
        record->text_history_3 = -1;
        record->text_history_2 = -1;
        record->text_history_1 = -1;
        record->text_history_0 = -1;
        record->picture_id = -1;
        g_main_saved_records[i].background_set = -1;
        record->sound_mode = 0;
        record->music_selection = 0;
        record->state_flags = 0;
        g_main_saved_records[i].section = 0;
        g_main_saved_records[i].counter = 0;
        record->_unknown_18 = 0;
        record->text_id = 0;
        g_main_saved_records[i].counter_delta = 0;
        g_main_saved_records[i].buffer = g_main_save_slot_buffer;
    }
    g_main_saved_records[0].brightness = 0x10;
    g_main_saved_records[1].brightness = 0x10;
    g_main_saved_records[2].brightness = 0;
    g_main_saved_records[3].brightness = 0;
    g_main_saved_records[4].brightness = 0x10;
    memcpy(g_main_brave_story_character_ages, g_main_brave_story_ages_source, 0x40);
    for (i = 0; i < 40; i++) {
        g_main_saved_data_bits[i] = 0;
    }
    for (i = 0; i < 2; i++) {
        g_main_secondary_saved_data_bits[i] = 0;
    }
    for (i = 0; i < 96; i++) {
        g_main_proposition_states[i] = 0;
    }
    g_main_save_proposition_count = 0;
    g_main_system_flags = 0;
}
