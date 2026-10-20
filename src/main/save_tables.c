/* Resetting the save record fields and the new game data tables. */

#include "fft/brave_story.h"
#include "fft/proposition.h"
#include "fft/save.h"
#include "fft/sound_novel.h"
#include "fft/system.h"
#include "psx/libc.h"
#include "psx/types.h"

sound_novel_reading_t g_sound_novel_saved_readings[5] = { 0 };

extern u8* g_brave_story_ages_source;

/* Reset the save record fields and new-game data tables: the five saved
 * sound-novel states (no text history, picture or background, zero counters
 * and flags, each on the shared save-slot buffer, brightness 0x10 except in
 * records 2 and 3), the Brave Story character ages from their template, the
 * saved data bits, the proposition states and count, and the system flags.
 *
 * Assembled with ASPSX < 2.30: the slot loop keeps five 4-instruction
 * `lui $at / addiu $at / addu $at,idx / sh 0($at)` forms, so this function
 * carries the per-function gcc-2.6.3_O2_aspsx-2.21 profile. */
void save_data_tables_init(void) {
    s32 i;
    sound_novel_reading_t* record;

    for (i = 0; i < 5; i++) {
        record = &g_sound_novel_saved_readings[i];
        record->text_history[3] = -1;
        record->text_history[2] = -1;
        record->text_history[1] = -1;
        record->text_history[0] = -1;
        record->picture_id = -1;
        g_sound_novel_saved_readings[i].background_set = -1;
        record->sound_mode = SOUND_NOVEL_SOUND_MODE_EFFECTS_ONLY;
        record->music_selection = 0;
        record->state_flags = 0;
        g_sound_novel_saved_readings[i].section = 0;
        g_sound_novel_saved_readings[i].counter = 0;
        record->_unknown_18 = 0;
        record->text_id = 0;
        g_sound_novel_saved_readings[i].counter_delta = 0;
        g_sound_novel_saved_readings[i].buffer = &g_sound_novel_script;
    }
    g_sound_novel_saved_readings[0].brightness = 0x10;
    g_sound_novel_saved_readings[1].brightness = 0x10;
    g_sound_novel_saved_readings[2].brightness = 0;
    g_sound_novel_saved_readings[3].brightness = 0;
    g_sound_novel_saved_readings[4].brightness = 0x10;
    memcpy(g_brave_story_character_ages, g_brave_story_ages_source, sizeof(g_brave_story_character_ages));
    for (i = 0; i < 40; i++) {
        g_sound_novel_read_section_bits[i] = 0;
    }
    for (i = 0; i < 2; i++) {
        g_sound_novel_saved_flags[i] = 0;
    }
    for (i = 0; i < 96; i++) {
        g_proposition_states[i] = 0;
    }
    g_proposition_count = 0;
    g_system_flags = 0;
}
