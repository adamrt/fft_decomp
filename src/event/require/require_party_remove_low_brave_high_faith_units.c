#include "fft/event_require.h"
#include "fft/thread.h"
#include "psx/types.h"

/*
 * Thread body of the post-battle desertions.
 *
 * Exits at once unless require_party_find_low_brave_high_faith_departures
 * found someone. Otherwise it binds the Brave/Faith text sections, shows the
 * banner and, for each departing unit in turn, plays its farewell line beside
 * the "left the party" banner for its gender, waits half a second and
 * removes the unit from the roster.
 */
void require_party_remove_low_brave_high_faith_units(void) {
    s32 i;
    s32 main_thread;

    if (require_party_find_low_brave_high_faith_departures() == 0)
        thread_exit_current();
    g_text_section_pointers[REQUIRE_TEXT_SECTION_BRAVE_FAITH_WARNINGS]
        = g_text_data + g_require_text_section_offsets[1];
    g_text_section_pointers[REQUIRE_TEXT_SECTION_BRAVE_FAITH_DEPARTURES]
        = g_text_data + g_require_text_section_offsets[2];
    g_text_section_pointers[REQUIRE_TEXT_SECTION_DISMISS_UNIT_REPLIES]
        = g_text_data + g_require_text_section_offsets[3];
    g_text_section_pointers[TEXT_SECTION_JOB_NAMES] = g_text_data + g_require_text_section_offsets[4];
    thread_yield();
    main_thread = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
    thread_start(main_thread, require_condition_run_special_thread);
    thread_set_parameters(main_thread, 6, 0, 0);
    require_gfx_fade_rgb_31_frames(0xd, 5, 2);
    thread_yield();
    for (i = 0; i < g_require_party_affected_unit_count; i++) {
        unit_stats_t* unit;
        s32 unit_thread = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
        s32 message_thread;
        g_text_substitution_values[0] = g_require_party_affected_battle_unit_indices[i];
        thread_start(unit_thread, text_run_dialog_thread_variant);
        thread_set_parameters(unit_thread, 0x1b, g_require_party_affected_unit_message_ids[i],
            g_require_party_affected_unit_message_args[i]);
        unit = battle_unit_get_stats_from_battle_id(g_require_party_affected_battle_unit_indices[i]);
        message_thread = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
        thread_start(message_thread, require_condition_run_special_thread);
        if (unit->unit_flags & UNIT_FLAG_FEMALE) {
            thread_set_parameters(message_thread, 9, 1, 0);
        } else {
            thread_set_parameters(message_thread, 8, 1, 0);
        }
        thread_wait_until_inactive(unit_thread);
        thread_set_parameters(message_thread, 0, 0, 2);
        thread_wait_until_inactive(message_thread);
        thread_wait_frames(0x1e);
        main_party_remove_unit(unit->formation_index);
    }
    thread_set_parameters(main_thread, 6, 0, 2);
    thread_wait_until_inactive(main_thread);
    require_gfx_fade_rgb_31_frames(6, 5, 2);
    thread_exit_current();
}
