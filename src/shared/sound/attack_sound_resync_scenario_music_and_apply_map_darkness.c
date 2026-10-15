#include "fft/battle_script.h"
#include "fft/event_attack.h"
#include "fft/main.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Re-syncs the two battle scenario-music slots with the pending track pair,
 * then applies the map darkness record.
 *
 * Skipped entirely when g_battle_scenario_event_active is clear or script variable 0x27 selects a
 * scenario in 410..425. When the currently playing id still appears in the
 * pending pair only that slot is reopened; otherwise both tracks restart.
 * `playing` is s32 rather than u16 because a u16 merge of the two lhu results
 * adds an andi 0xffff that the target does not have. */
void attack_sound_resync_scenario_music_and_apply_map_darkness(void) {
    CVECTOR darkness = g_attack_map_darkness_color;
    s32 scenario;
    s32 playing;
    u16 replacement;

    scenario = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
    if (g_script_scenario_event_active != 0 && (scenario < 410 || scenario > 425)) {
        if (g_sound_music_track_1_id == 0 && g_sound_music_track_2_id == 0) {
            g_sound_music_track_index = 0;
            sound_play_music_tracks(g_battle_scenario_music_1_id, g_battle_scenario_music_2_id);
        } else {
            if (g_sound_music_track_index == 0) {
                playing = g_sound_music_track_1_id;
                replacement = g_battle_scenario_music_1_id;
            } else {
                playing = g_sound_music_track_2_id;
                replacement = g_battle_scenario_music_2_id;
            }

            if (playing != g_battle_scenario_music_1_id && playing != g_battle_scenario_music_2_id) {
                g_sound_music_track_index = 0;
                sound_play_music_tracks(g_battle_scenario_music_1_id, g_battle_scenario_music_2_id);
            } else {
                g_thread_call_target = (void (*)(void))main_sound_unload_scenario_mus;
                thread_call_on_main_stack((g_sound_music_track_index ^ 1) + 1);
                g_thread_call_target = (void (*)(void))main_sound_open_and_play_music;
                thread_call_on_main_stack(replacement, (g_sound_music_track_index ^ 1) + 1);
                sound_wait_for_music_stub();
            }
        }

        g_sound_music_track_1_id = g_battle_scenario_music_1_id;
        g_sound_music_track_2_id = g_battle_scenario_music_2_id;
    }

    map_light_state_command(MAP_LIGHT_COMMAND_SET_DARKNESS_COLOR, &darkness);
    text_build_unit_name_list();
}
