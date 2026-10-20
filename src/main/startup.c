/* Power-on and fresh-session setup. The first boot starts the hardware and
 * shows both logos; later session resets reload the shared resources under
 * the Squaresoft logo. */

#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/item.h"
#include "fft/memcard.h"
#include "fft/memory.h"
#include "fft/menu_sort.h"
#include "fft/music.h"
#include "fft/save.h"
#include "fft/sound.h"
#include "fft/status.h"
#include "fft/system.h"
#include "fft/zodiac.h"
#include "psx/libapi.h"
#include "psx/libc.h"
#include "psx/libcd.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libspu.h"
#include "psx/types.h"

static s32 g_session_initialized = 0;

/* Set the RAM size to 2 MB, initialize the CD library with debug output off, pause the drive and
 * mark the shared file descriptor idle. */
static void cd_memory_init(void) {
    SetMem(2);
    CdInit();
    CdSetDebug(0);
    file_cd_pause_reset(&g_file_cd_state);
    g_file_cd_state.state = FILE_LOAD_STATE_IDLE;
}

/* BATTLE/ZODIAC.BIN is a headerless image, not an executable overlay.
 * Sector 0xec61 supplies 0x10000 bytes: 128 * 256 16-bit VRAM transfer
 * values at (640, 0), subsequently rendered as 8-bit indexed texture data. */
void zodiac_frame_load(void) {
    RECT destination = { 640, 0, 128, 256 };
    u32* image = file_heap_load_checked(DISC_SECTOR_BATTLE_ZODIAC_BIN, 0x10000);

    LoadImage(&destination, image);
    DrawSync(0);
    heap_free(image);
    gfx_frame_texture_load();
}

/* Power-on initialization: install the VSync, DrawSync and CD callbacks, start
 * the GPU, pad, SPU and CD, show the two logos, open the memory-card events and
 * load the shared sounds and the zodiac frame while the Squaresoft logo is up.
 *
 * rand is seeded with 1, and g_session_initialized is cleared so the first
 * boot_session_reset does not repeat the hardware restart. */
void boot_startup_run(void) {
    heap_game_table_clear();
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
    cd_memory_init();
    gfx_display_reset(256, 240, 512, 0, 0, 0);
    boot_sceap_logo_show();
    boot_squaresoft_logo_fade_in();
    memcard_events_init();
    sound_init();
    zodiac_frame_load();
    srand(1);
    boot_squaresoft_logo_fade_out();
    g_session_initialized = 0;
}

/* Prepare a fresh session before the opening runs: reset the 32-bit save values and
 * options, rebuild the status check sets and restore the save data and item order
 * tables.
 *
 * On the first pass after power-on only that is done, because
 * boot_startup_run has just shown the logos. On every later pass (a soft
 * reset or the end of the game) it first frees the game heap, unloads the
 * scenario music, zeroes the play time, restarts the GPU, SPU and CD, and shows
 * the Squaresoft logo again for at least 60 frames while the shared sounds and
 * the zodiac frame reload. */
void boot_session_reset(void) {
    int frame;

    if (g_session_initialized != 0) {
        heap_game_table_clear();
        music_all_unload();
        g_save_play_time_hours = 0;
        g_save_play_time_minutes = 0;
        g_save_play_time_seconds = 0;
        g_save_play_time_frames = 0;
        ResetGraph(1);
        SetGraphDebug(0);
        SpuInitHot();
        cd_memory_init();
        gfx_display_reset(256, 240, 512, 0, 0, 0);
        boot_squaresoft_logo_fade_in();
        sound_init();
        zodiac_frame_load();
    }

    save_state_init();
    status_check_data_init();
    save_data_tables_init();
    menu_sort_tables_init();

    if (g_session_initialized != 0) {
        for (frame = 0; frame < 60; frame++) {
            VSync(0);
        }
        boot_squaresoft_logo_fade_out();
    }

    g_session_initialized = 1;
}
