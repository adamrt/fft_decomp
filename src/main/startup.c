/* Power-on and fresh-session setup. The first boot starts the hardware and
 * shows both logos; later session resets reload the shared resources under
 * the Squaresoft logo. */

#include "fft/game/status.h"
#include "fft/gfx/gfx.h"
#include "fft/gfx/zodiac.h"
#include "fft/menu/menu_sort.h"
#include "fft/sys/file.h"
#include "fft/sys/memcard.h"
#include "fft/sys/memory.h"
#include "fft/sys/music.h"
#include "fft/sys/sound.h"
#include "fft/sys/system.h"
#include "fft/world/save.h"
#include "psx/libapi.h"
#include "psx/libc.h"
#include "psx/libcd.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libspu.h"
#include "psx/types.h"

static s32 s_session_initialized = 0;

/* Set the RAM size to 2 MB, initialize the CD library with debug output off, pause the drive and
 * mark the shared file descriptor idle. */
static void init_memory_and_cd(void) {
    SetMem(2);
    CdInit();
    CdSetDebug(0);
    file_cd_pause(&g_file_cd_state);
    g_file_cd_state.state = FILE_LOAD_STATE_IDLE;
}

/* BATTLE/ZODIAC.BIN is a headerless image, not an executable overlay.
 * Sector 0xec61 supplies 0x10000 bytes: 128 * 256 16-bit VRAM transfer
 * values at (640, 0), subsequently rendered as 8-bit indexed texture data. */
void gfx_load_zodiac_and_frame_textures(void) {
    RECT destination = { 640, 0, 128, 256 };
    u32* image = file_load_into_heap_checked(DISC_SECTOR_BATTLE_ZODIAC_BIN, 0x10000);

    LoadImage(&destination, image);
    DrawSync(0);
    heap_free(image);
    gfx_load_frame_texture();
}

/* Power-on initialization: install the VSync, DrawSync and CD callbacks, start
 * the GPU, pad, SPU and CD, show the two logos, open the memory-card events and
 * load the shared sounds and the zodiac frame while the Squaresoft logo is up.
 *
 * rand is seeded with 1, and g_session_initialized is cleared so the first
 * boot_reset_session does not repeat the hardware restart. */
void boot_run_startup(void) {
    heap_clear_game_table();
    ResetCallback();

    g_save_play_time_hours = 0;
    g_save_play_time_minutes = 0;
    g_save_play_time_seconds = 0;
    g_save_play_time_frames = 0;

    VSyncCallback(system_vsync_callback);
    DrawSyncCallback(system_draw_sync_callback);
    CdReadyCallback(file_cd_ready_callback);
    CdReadCallback(file_cd_read_callback);

    ResetGraph(0);
    SetGraphDebug(0);
    PadInit(0);
    SpuInit();
    init_memory_and_cd();
    gfx_reset_display(256, 240, 512, 0, 0, 0);
    boot_show_sceap_logo();
    boot_fade_in_squaresoft_logo();
    memcard_init_events();
    sound_init();
    gfx_load_zodiac_and_frame_textures();
    srand(1);
    boot_fade_out_squaresoft_logo();
    s_session_initialized = 0;
}

/* Prepare a fresh session before the opening runs: reset the 32-bit save values and
 * options, rebuild the status check sets and restore the save data and item order
 * tables.
 *
 * On the first pass after power-on only that is done, because
 * boot_run_startup has just shown the logos. On every later pass (a soft
 * reset or the end of the game) it first frees the game heap, unloads the
 * scenario music, zeroes the play time, restarts the GPU, SPU and CD, and shows
 * the Squaresoft logo again for at least 60 frames while the shared sounds and
 * the zodiac frame reload. */
void boot_reset_session(void) {
    int frame;

    if (s_session_initialized != 0) {
        heap_clear_game_table();
        music_unload_all();
        g_save_play_time_hours = 0;
        g_save_play_time_minutes = 0;
        g_save_play_time_seconds = 0;
        g_save_play_time_frames = 0;
        ResetGraph(1);
        SetGraphDebug(0);
        SpuInitHot();
        init_memory_and_cd();
        gfx_reset_display(256, 240, 512, 0, 0, 0);
        boot_fade_in_squaresoft_logo();
        sound_init();
        gfx_load_zodiac_and_frame_textures();
    }

    save_init_state();
    status_init_check_data();
    save_init_data_tables();
    menu_init_sort_tables();

    if (s_session_initialized != 0) {
        for (frame = 0; frame < 60; frame++) {
            VSync(0);
        }
        boot_fade_out_squaresoft_logo();
    }

    s_session_initialized = 1;
}
