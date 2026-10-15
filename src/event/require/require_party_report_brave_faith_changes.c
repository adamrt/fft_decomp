#include "fft/battle_script.h"
#include "fft/event_require.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/types.h"

/*
 * Thread body of the post-battle Brave/Faith report.
 *
 * Exits at once in the world debug battle style, or when
 * require_party_update_brave_faith_and_collect_warnings reports
 * nobody. Otherwise it binds the Brave/Faith text sections, shows banner 5
 * and plays each affected unit's warning line in turn before fading the
 * banner out.
 */
void require_party_report_brave_faith_changes(void) {
    s32 i;
    s32 main_thread;

    if (script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0)
        thread_exit_current();
    if (require_party_update_brave_faith_and_collect_warnings() == 0)
        thread_exit_current();
    g_text_section_pointers[REQUIRE_TEXT_SECTION_BRAVE_FAITH_WARNINGS]
        = g_text_data + g_require_text_section_offsets[1];
    g_text_section_pointers[REQUIRE_TEXT_SECTION_BRAVE_FAITH_DEPARTURES]
        = g_text_data + g_require_text_section_offsets[2];
    g_text_section_pointers[REQUIRE_TEXT_SECTION_DISMISS_UNIT_REPLIES]
        = g_text_data + g_require_text_section_offsets[3];
    g_text_section_pointers[TEXT_SECTION_JOB_NAMES] = g_text_data + g_require_text_section_offsets[4];
    main_thread = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
    thread_start(main_thread, require_condition_run_special_thread);
    thread_set_parameters(main_thread, 5, 0, 0);
    require_gfx_fade_rgb_31_frames(0xd, 0xd, 2);
    for (i = 0; i < g_require_party_affected_unit_count; i++) {
        s32 unit_thread = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
        g_text_substitution_values[0] = g_require_party_affected_battle_unit_indices[i];
        thread_start(unit_thread, text_run_dialog_thread_variant);
        thread_set_parameters(unit_thread, 0x1b, g_require_party_affected_unit_message_ids[i],
            g_require_party_affected_unit_message_args[i]);
        thread_wait_until_inactive(unit_thread);
    }
    thread_set_parameters(main_thread, 0, 0, 2);
    thread_wait_until_inactive(main_thread);
    require_gfx_fade_rgb_31_frames(6, 5, 2);
    thread_exit_current();
}
