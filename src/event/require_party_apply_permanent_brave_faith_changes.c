#include "fft/event_require.h"
#include "psx/types.h"

void require_party_apply_permanent_brave_faith_changes(void) {
    s32 i;
    s32 main_thread;

    if (battle_script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0)
        battle_thread_exit_current();
    if (require_apply_permanent_brave_faith_changes_and_collect_warnings() == 0)
        battle_thread_exit_current();
    g_battle_text_section_pointers[1] = g_require_text_data + g_require_text_section_offsets[1];
    g_battle_text_section_pointers[2] = g_require_text_data + g_require_text_section_offsets[2];
    g_battle_text_section_pointers[3] = g_require_text_data + g_require_text_section_offsets[3];
    g_battle_text_section_pointers[6] = g_require_text_data + g_require_text_section_offsets[4];
    /* The target also passes the text data pointer in a1 to this one-argument callee. */
    main_thread = ((s32 (*)())battle_thread_resolve_id)(0x10, g_require_text_data);
    battle_thread_start(main_thread, require_render_display_condition_special_cases_thread);
    battle_thread_set_parameters(main_thread, 5, 0, 0);
    require_gfx_fade_rgb_31_frames(0xd, 0xd, 2);
    for (i = 0; i < g_require_party_affected_unit_count; i++) {
        s32 unit_thread = battle_thread_resolve_id(0x10);
        g_battle_text_substitution_values[0] = g_require_party_affected_battle_unit_indices[i];
        battle_thread_start(unit_thread, battle_text_character_handling_thread);
        battle_thread_set_parameters(unit_thread, 0x1b, g_require_party_affected_unit_message_ids[i],
            g_require_party_affected_unit_message_args[i]);
        battle_thread_wait_until_inactive(unit_thread);
    }
    battle_thread_set_parameters(main_thread, 0, 0, 2);
    battle_thread_wait_until_inactive(main_thread);
    require_gfx_fade_rgb_31_frames(6, 5, 2);
    battle_thread_exit_current();
}
