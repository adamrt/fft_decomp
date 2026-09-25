#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Advance egg hatch counters, then make the daily monster-breeding roll.
 *
 * A counter that reaches 0 hatches as a monster born on the current date.
 * A successful roll selects a random roster monster and one of the three
 * offspring jobs in its Chocobo-based breeding-table entry. The new egg's
 * hatch range comes from its offspring entry, and its level is based on a
 * random non-egg roster member. */
void world_unit_update_monster_breeding(s32 unused) {
    u8 monsters[0x18];
    u8 levels[0x18];
    s32 monster_count = 0;
    s32 unit_count = 0;
    s32 i = 0;
    /* The original reused two scalar temporaries throughout; splitting them
     * into typed locals changes the callee-saved allocation (n holds the
     * egg counter, month index, rarity class, offspring job and new slot;
     * value holds the birthday, the random draw and the saved name-buffer
     * pointer). */
    s32 value;
    s32 n;
    s32 day;
    s32 month;
    u8** name_buffer;
    party_data_t* unit;

    srand(g_main_system_play_time_frames * 100 + g_main_system_play_time_seconds * 10 + g_main_system_play_time_minutes
        + g_main_system_play_time_hours + g_main_system_play_time_frames);
    do {
        unit = main_party_get_data_pointer(i);
        if (unit->party_id != PARTY_ID_NONE) {
            if (unit->gender_flags & UNIT_FLAG_EGG) {
                n = unit->birthday_day & 0xF;
                n -= 1;
                unit->birthday_day = (unit->birthday_day & 0xF0) | n;
                if (n == 0) {
                    unit->gender_flags = UNIT_FLAG_MONSTER;
                    day = world_script_get_variable(EVENT_SCRIPT_VAR_DAY);
                    month = world_script_get_variable(EVENT_SCRIPT_VAR_MONTH);
                    value = 0;
                    for (n = 1; n < month; n++) {
                        value += g_world_month_lengths[n];
                    }
                    value += day;
                    value |= world_unit_get_zodiac_for_date(month, day) << 12;
                    unit->birthday_day = value;
                    unit->zodiac = value >> 8;
                }
            } else {
                levels[unit_count++] = unit->level;
                if ((u32)(world_job_get_skillset(unit->job_id) - SKILLSET_ID_MONSTER_FIRST)
                    < SKILLSET_ID_END - SKILLSET_ID_MONSTER_FIRST) {
                    monsters[monster_count++] = i;
                }
            }
        }
        i++;
    } while (i < PARTY_ROSTER_SLOT_COUNT);

    if (monster_count == 0) {
        return;
    }
    value = rand() & 0xFF;
    if (value < 6) {
        n = WORLD_MONSTER_BREED_RARITY_RARE;
    } else if (value < 0x10) {
        n = WORLD_MONSTER_BREED_RARITY_UNCOMMON;
    } else {
        n = value < 0x20;
    }
    if (n == WORLD_MONSTER_BREED_RARITY_NONE) {
        return;
    }
    monster_count = rand() % monster_count;
    n = g_world_unit_monster_breed_table[main_party_get_data_pointer(monsters[monster_count])->job_id - JOB_ID_CHOCOBO]
            .offspring_job_by_rarity[n - WORLD_MONSTER_BREED_RARITY_COMMON];
    i = rand() % g_world_unit_monster_breed_table[n - JOB_ID_CHOCOBO].hatch_range + 1;
    if (i < 2) {
        i = 2;
    }
    name_buffer = &g_world_text_roster_unit_names;
    value = (s32)*name_buffer;
    *name_buffer = g_world_text_unit_names;
    n = main_party_create_monster_egg(n, i + i * 0x10, rand() % 4);
    *name_buffer = (u8*)value;
    if (n != -1) {
        unit = main_party_get_data_pointer(n);
        main_party_level_unit_to_target(unit, levels[rand() % unit_count] - 1);
    }
}
