#include "fft/attack.h"
#include "fft/battle.h"
#include "fft/event.h"
#include "fft/main_sound.h"
#include "fft/map.h"
#include "fft/script_variables.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Four unaligned bytes at the head of ATTACK.OUT; only the first three reach
 * the map darkness record through selector 0x63. */
typedef struct attack_map_darkness {
    u8 data[4];
} attack_map_darkness_t;

extern attack_map_darkness_t attack_D_801BF000;

/* Re-syncs the two battle scenario-music slots with the pending track pair,
 * then applies the map darkness record.
 *
 * Skipped entirely when g_battle_scenario_event_active is clear or script variable 0x27 selects a
 * scenario in 410..425. When the currently playing id still appears in the
 * pending pair only that slot is reopened; otherwise both tracks restart.
 * `playing` is s32 rather than u16 because a u16 merge of the two lhu results
 * adds an andi 0xffff that the target does not have. */
void attack_sound_resync_scenario_music_and_apply_map_darkness(void) {
    attack_map_darkness_t darkness = attack_D_801BF000;
    s32 scenario;
    s32 playing;
    u16 replacement;

    scenario = battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
    if (g_battle_scenario_event_active != 0 && (u32)(scenario - 410) >= 16) {
        if (g_battle_music_track_1_id == 0 && g_battle_music_track_2_id == 0) {
            g_battle_current_music_track_index = 0;
            attack_sound_play_scenario_music(g_battle_scenario_music_1_id, g_battle_scenario_music_2_id);
        } else {
            if (g_battle_current_music_track_index == 0) {
                playing = g_battle_music_track_1_id;
                replacement = g_battle_scenario_music_1_id;
            } else {
                playing = g_battle_music_track_2_id;
                replacement = g_battle_scenario_music_2_id;
            }

            if (playing != g_battle_scenario_music_1_id && playing != g_battle_scenario_music_2_id) {
                g_battle_current_music_track_index = 0;
                attack_sound_play_scenario_music(g_battle_scenario_music_1_id, g_battle_scenario_music_2_id);
            } else {
                g_battle_thread_call_target = (void (*)(void))main_sound_unload_scenario_mus;
                battle_thread_call_on_main_stack((g_battle_current_music_track_index ^ 1) + 1);
                g_battle_thread_call_target = (void (*)(void))main_sound_open_and_play_music;
                battle_thread_call_on_main_stack(replacement, (g_battle_current_music_track_index ^ 1) + 1);
                attack_sound_wait_music_idle();
            }
        }

        g_battle_music_track_1_id = g_battle_scenario_music_1_id;
        g_battle_music_track_2_id = g_battle_scenario_music_2_id;
    }

    battle_map_light_state_command(MAP_LIGHT_COMMAND_SET_DARKNESS_COLOR, &darkness);
    battle_text_build_unit_name_list();
}
