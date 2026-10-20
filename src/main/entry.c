/* The resident loop alternates the title sequence, world map and battles.
 * Soft reset restores its saved stack and starts the loop again. */

#include "fft/file.h"
#include "fft/item.h"
#include "fft/memory.h"
#include "fft/music.h"
#include "fft/overlay.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/state.h"
#include "fft/suzuki.h"
#include "fft/system.h"
#include "fft/title.h"
#include "fft/world_map.h"
#include "fft/zodiac.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libspu.h"

/* Resident title route flag, consumed and cleared during ATTACK's scenario setup. */
u8 g_system_go_straight_to_battle = 1;
extern u32 g_system_game_loop_stack_pointer;

/* Let the GPU finish, blank the display and shut down the sound driver and the SPU. */
static void display_audio_stop(void) {
    DrawSync(0);
    DrawSync(0);
    SetDispMask(0);
    suzuki_sound_quit();
    SpuQuit();
}

/* Soft reset: stop the display and audio, then restart system_game_loop_run
 * on the stack pointer main saved. */
void system_game_reset(void) {
    display_audio_stop();
    system_game_loop_stack_restore(&g_system_game_loop_stack_pointer);
}

/* Run MAIN's no-op region and return to the BATTLE loader. */
static void padding_call(void) {
    system_zero_padding();
}

/* Load BATTLE.BIN (sector 1000) into the whole low overlay area, which ends at
 * the high overlay address, and call system_zero_padding. That is not BATTLE
 * code but MAIN's 4 KB of zero words ending in `jr ra`; BATTLE runs when the
 * caller then calls battle_entrypoint. */
static void battle_overlay_load(void) {
    file_buffer_load_checked(DISC_SECTOR_BATTLE_BIN, g_heap_high_overlay_load_address - g_heap_low_overlay_load_address,
        g_heap_low_overlay_load_address);
    padding_call();
}

/* The top-level loop of the game: opening, then world map and battles until a
 * reset or the ending. It never returns; system_game_reset re-enters it from
 * the top with the stack restored.
 *
 * Each pass empties the inventory to the new-game stock and runs OPEN.BIN (after
 * boot_session_reset, unless the world map asked for the title screen
 * with WORLD_MAP_RESULT_TITLE, which also skips the opening movie). An OPEN.BIN result of 0
 * goes straight into BATTLE.BIN; otherwise WLDCORE and WORLD run first and leave
 * their result in g_system_frontend_world_result. World map and BATTLE.BIN
 * then alternate until g_game_flow_state asks for a reset or the
 * ending. The ending seeds ATTACK's post-funeral route, plays OPEN.BIN's ending
 * cutscene and runs BATTLE.BIN once more to enter the epilogue. */
void system_game_loop_run(void) {
    int open_mode;

    g_system_frontend_world_result = WORLD_MAP_RESULT_NONE;

    for (;;) {
        item_new_game_inventory_init();

        if (g_system_frontend_world_result == WORLD_MAP_RESULT_TITLE) {
            open_mode = 1;
        } else {
            boot_session_reset();
            open_mode = 0;
        }

        if (overlay_title_run(open_mode) == TITLE_RESULT_BATTLE) {
            g_system_go_straight_to_battle = 1;
            g_system_frontend_world_result = WORLD_MAP_RESULT_NONE;
        } else {
            g_system_go_straight_to_battle = 0;
        }

        do {
            if (g_system_go_straight_to_battle == 0) {
                g_system_frontend_world_result = overlay_world_run(1);
            }

            if (g_system_frontend_world_result == WORLD_MAP_RESULT_TITLE) {
                break;
            }

            battle_overlay_load();
            battle_entrypoint();
            sound_all_sfx_stop();
            g_system_go_straight_to_battle = 0;

            if (g_game_flow_state == SYSTEM_GAME_FLOW_RESET) {
                display_audio_stop();
                break;
            }
        } while (g_game_flow_state != SYSTEM_GAME_FLOW_ENDING);

        if (g_game_flow_state == SYSTEM_GAME_FLOW_ENDING) {
            sound_all_sfx_stop();
            music_all_unload();
            script_variable_set(EVENT_SCRIPT_VAR_NEXT_SCENARIO, 1);
            script_variable_set(EVENT_SCRIPT_VAR_CURRENT_EVENT, EVENT_ID_THE_LYING_HEART);
            overlay_ending_run();
            battle_overlay_load();
            zodiac_frame_load();
            battle_entrypoint();
            display_audio_stop();
        }
    }
}

/* Program entry: run the power-on startup, save the stack pointer for
 * system_game_reset and enter the game loop. The loop never returns, so the
 * pad and callback shutdown below it never runs. */
void main(void) {
    boot_startup_run();
    system_stack_pointer_store(&g_system_game_loop_stack_pointer);
    system_game_loop_run();
    PadStop();
    StopCallback();
}
