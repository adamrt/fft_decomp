#include "fft/battle.h"
#include "fft/battle_text.h"
#include "fft/require.h"
#include "psx/types.h"

void require_party_remove_low_brave_high_faith_units(void) {
    s32 i;
    s32 main_thread;

    if (require_party_find_low_brave_high_faith_departures() == 0)
        battle_thread_exit_current();
    g_battle_text_section_pointers[1] = g_require_text_data + g_require_text_section_offsets[1];
    g_battle_text_section_pointers[2] = g_require_text_data + g_require_text_section_offsets[2];
    g_battle_text_section_pointers[3] = g_require_text_data + g_require_text_section_offsets[3];
    g_battle_text_section_pointers[6] = g_require_text_data + g_require_text_section_offsets[4];
    battle_thread_yield();
    main_thread = battle_thread_resolve_id(0x10);
    battle_thread_start(main_thread, require_render_display_condition_special_cases_thread);
    battle_thread_set_parameters(main_thread, 6, 0, 0);
    require_gfx_fade_rgb_31_frames(0xd, 5, 2);
    battle_thread_yield();
    for (i = 0; i < g_require_party_affected_unit_count; i++) {
        battle_stats_t* unit;
        s32 unit_thread = battle_thread_resolve_id(0x10);
        s32 message_thread;
        g_battle_text_substitution_values[0] = g_require_party_affected_battle_unit_indices[i];
        battle_thread_start(unit_thread, battle_text_character_handling_thread);
        battle_thread_set_parameters(unit_thread, 0x1b, g_require_party_affected_unit_message_ids[i],
            g_require_party_affected_unit_message_args[i]);
        unit = battle_unit_get_stats_from_battle_id(g_require_party_affected_battle_unit_indices[i]);
        message_thread = battle_thread_resolve_id(0x10);
        battle_thread_start(message_thread, require_render_display_condition_special_cases_thread);
        if (unit->unit_flags & UNIT_FLAG_FEMALE) {
            battle_thread_set_parameters(message_thread, 9, 1, 0);
        } else {
            battle_thread_set_parameters(message_thread, 8, 1, 0);
        }
        battle_thread_wait_until_inactive(unit_thread);
        battle_thread_set_parameters(message_thread, 0, 0, 2);
        battle_thread_wait_until_inactive(message_thread);
        battle_thread_wait_frames(0x1e);
        main_party_remove_unit(unit->formation_index);
    }
    battle_thread_set_parameters(main_thread, 6, 0, 2);
    battle_thread_wait_until_inactive(main_thread);
    require_gfx_fade_rgb_31_frames(6, 5, 2);
    battle_thread_exit_current();
}
