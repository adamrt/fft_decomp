/* The resident loop alternates the title sequence, world map and battles.
 * Soft reset restores its saved stack and starts the loop again. */

#include "fft/battle/battle_state.h"
#include "fft/front/title.h"
#include "fft/game/item.h"
#include "fft/gfx/zodiac.h"
#include "fft/script/script_variable.h"
#include "fft/sys/file.h"
#include "fft/sys/memory.h"
#include "fft/sys/music.h"
#include "fft/sys/overlay.h"
#include "fft/sys/sound.h"
#include "fft/sys/suzuki.h"
#include "fft/sys/system.h"
#include "fft/world/world_map.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libspu.h"

/* Resident title route flag, consumed and cleared during ATTACK's scenario setup. */
u8 g_system_go_straight_to_battle = 1;
static u32 s_system_game_loop_stack_pointer;

/* Let the GPU finish, blank the display and shut down the sound driver and the SPU. */
static void stop_display_and_audio(void) {
    DrawSync(0);
    DrawSync(0);
    SetDispMask(0);
    suzuki_sound_quit();
    SpuQuit();
}

/* Soft reset: stop the display and audio, then restart system_run_game_loop
 * on the stack pointer main saved. */
void system_reset_game(void) {
    stop_display_and_audio();
    system_restore_game_loop_stack(&s_system_game_loop_stack_pointer);
}

/* Run MAIN's no-op region and return to the BATTLE loader. */
static void call_padding(void) {
    system_zero_padding();
}

/* Load BATTLE.BIN (sector 1000) into the whole low overlay area, which ends at
 * the high overlay address, and call system_zero_padding. That is not BATTLE
 * code but MAIN's 4 KB of zero words ending in `jr ra`; BATTLE runs when the
 * caller then calls battle_entrypoint. */
static void load_battle_overlay(void) {
    file_load_into_buffer_checked(DISC_SECTOR_BATTLE_BIN,
        g_heap_high_overlay_load_address - g_heap_low_overlay_load_address, g_heap_low_overlay_load_address);
    call_padding();
}

/* The top-level loop of the game: opening, then world map and battles until a
 * reset or the ending. It never returns; system_reset_game re-enters it from
 * the top with the stack restored.
 *
 * Each pass empties the inventory to the new-game stock and runs OPEN.BIN (after
 * boot_reset_session, unless the world map asked for the title screen
 * with WORLD_MAP_RESULT_TITLE, which also skips the opening movie). An OPEN.BIN result of 0
 * goes straight into BATTLE.BIN; otherwise WLDCORE and WORLD run first and leave
 * their result in g_system_frontend_world_result. World map and BATTLE.BIN
 * then alternate until g_game_flow_state asks for a reset or the
 * ending. The ending seeds ATTACK's post-funeral route, plays OPEN.BIN's ending
 * cutscene and runs BATTLE.BIN once more to enter the epilogue. */
void system_run_game_loop(void) {
    int open_mode;

    g_system_frontend_world_result = WORLD_MAP_RESULT_NONE;

    for (;;) {
        item_init_new_game_inventory();

        if (g_system_frontend_world_result == WORLD_MAP_RESULT_TITLE) {
            open_mode = 1;
        } else {
            boot_reset_session();
            open_mode = 0;
        }

        if (overlay_run_title(open_mode) == TITLE_RESULT_BATTLE) {
            g_system_go_straight_to_battle = 1;
            g_system_frontend_world_result = WORLD_MAP_RESULT_NONE;
        } else {
            g_system_go_straight_to_battle = 0;
        }

        do {
            if (g_system_go_straight_to_battle == 0) {
                g_system_frontend_world_result = overlay_run_world(1);
            }

            if (g_system_frontend_world_result == WORLD_MAP_RESULT_TITLE) {
                break;
            }

            load_battle_overlay();
            battle_entrypoint();
            sound_stop_all_sfx();
            g_system_go_straight_to_battle = 0;

            if (g_game_flow_state == SYSTEM_GAME_FLOW_RESET) {
                stop_display_and_audio();
                break;
            }
        } while (g_game_flow_state != SYSTEM_GAME_FLOW_ENDING);

        if (g_game_flow_state == SYSTEM_GAME_FLOW_ENDING) {
            sound_stop_all_sfx();
            music_unload_all();
            script_variable_set(SCRIPT_VAR_NEXT_SCENARIO, 1);
            script_variable_set(SCRIPT_VAR_CURRENT_EVENT, EVENT_ID_THE_LYING_HEART);
            overlay_run_ending();
            load_battle_overlay();
            gfx_load_zodiac_and_frame_textures();
            battle_entrypoint();
            stop_display_and_audio();
        }
    }
}

/* Program entry: run the power-on startup, save the stack pointer for
 * system_reset_game and enter the game loop. The loop never returns, so the
 * pad and callback shutdown below it never runs. */
void main(void) {
    boot_run_startup();
    system_store_stack_pointer(&s_system_game_loop_stack_pointer);
    system_run_game_loop();
    PadStop();
    StopCallback();
}
