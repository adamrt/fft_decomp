/* CD-ROM and CPU RAM reset, the zodiac frame, startup and game state reset, ENTD files and encounters, new game
 * inventory and party, and SVECTOR setting. */

#include "fft/encounter.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/item.h"
#include "fft/memcard.h"
#include "fft/memory.h"
#include "fft/music.h"
#include "fft/party.h"
#include "fft/save.h"
#include "fft/screen.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/status.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/unit_entd.h"
#include "fft/zodiac.h"
#include "psx/libapi.h"
#include "psx/libc.h"
#include "psx/libcd.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/libspu.h"
#include "psx/types.h"

static s32 g_boot_startup2_has_run = 0;

/* Set the RAM size to 2 MB, initialize the CD library with debug output off, pause the drive and
 * mark the shared file descriptor idle. */
static void file_reset_cdrom_cpu_ram(void) {
    SetMem(2);
    CdInit();
    CdSetDebug(0);
    file_reset_pause_cdrom(&g_file_cd_state);
    g_file_cd_state.state = 0;
}

/* BATTLE/ZODIAC.BIN is a headerless image, not an executable overlay.
 * Sector 0xec61 supplies 0x10000 bytes: 128 * 256 16-bit VRAM transfer
 * values at (640, 0), subsequently rendered as 8-bit indexed texture data. */
void zodiac_load_frame(void) {
    RECT destination = { 640, 0, 128, 256 };
    u32* image = file_load_checked_to_heap(DISC_SECTOR_BATTLE_ZODIAC_BIN, 0x10000);

    LoadImage(&destination, image);
    DrawSync(0);
    heap_free(image);
    gfx_load_frame_bin();
}

/* Power-on initialization: install the VSync, DrawSync and CD callbacks, start
 * the GPU, pad, SPU and CD, show the two logos, open the memory-card events and
 * load the shared sounds and the zodiac frame while the Squaresoft logo is up.
 *
 * rand is seeded with 1, and g_boot_startup2_has_run is cleared so the first
 * boot_reset_game_state does not repeat the hardware restart. */
void boot_run_startup(void) {
    heap_clear_game_table();
    ResetCallback();

    g_save_play_time_hours = 0;
    g_save_play_time_minutes = 0;
    g_save_play_time_seconds = 0;
    g_save_play_time_frames = 0;

    VSyncCallback(system_handle_vsync);
    DrawSyncCallback(system_handle_draw_sync);
    CdReadyCallback(file_handle_cd_ready);
    CdReadCallback(file_handle_cd_read);

    ResetGraph(0);
    SetGraphDebug(0);
    PadInit(0);
    SpuInit();
    file_reset_cdrom_cpu_ram();
    gfx_reset_display(256, 240, 512, 0, 0, 0);
    boot_show_sceap_logo();
    boot_fade_in_squaresoft_logo();
    memcard_init_events();
    sound_open_generic_sfx();
    zodiac_load_frame();
    srand(1);
    boot_fade_out_squaresoft_logo();
    g_boot_startup2_has_run = 0;
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
void boot_reset_game_state(void) {
    int frame;

    if (g_boot_startup2_has_run != 0) {
        heap_clear_game_table();
        music_unload_scenario_and_tunes();
        g_save_play_time_hours = 0;
        g_save_play_time_minutes = 0;
        g_save_play_time_seconds = 0;
        g_save_play_time_frames = 0;
        ResetGraph(1);
        SetGraphDebug(0);
        SpuInitHot();
        file_reset_cdrom_cpu_ram();
        gfx_reset_display(256, 240, 512, 0, 0, 0);
        boot_fade_in_squaresoft_logo();
        sound_open_generic_sfx();
        zodiac_load_frame();
    }

    save_init_state_and_options();
    status_init_check_data();
    save_init_data_tables();
    item_init_order_tables();

    if (g_boot_startup2_has_run != 0) {
        for (frame = 0; frame < 60; frame++) {
            VSync(0);
        }
        boot_fade_out_squaresoft_logo();
    }

    g_boot_startup2_has_run = 1;
}

/* Start loading the ENTD file that holds g_encounter_current_event_id into a new
 * 0x14000-byte heap block (g_encounter_set). The four files cover 128 events
 * each.
 *
 * Returns 1 once the read is queued, and also for event 0, which loads nothing.
 * Returns 0 with the block freed when the loader is busy; the caller retries. */
int encounter_open_file(void) {
    int lba;
    entd_file_t* entd_set;

    if (g_encounter_current_event_id != 0) {
        if (g_encounter_current_event_id < ENTD2_FIRST_EVENT) {
            entd_set = heap_alloc(sizeof(entd_file_t));
            g_encounter_set = entd_set;
            lba = DISC_SECTOR_BATTLE_ENTD1_ENT;
        } else if (g_encounter_current_event_id < ENTD3_FIRST_EVENT) {
            entd_set = heap_alloc(sizeof(entd_file_t));
            g_encounter_set = entd_set;
            lba = DISC_SECTOR_BATTLE_ENTD2_ENT;
        } else if (g_encounter_current_event_id < ENTD4_FIRST_EVENT) {
            entd_set = heap_alloc(sizeof(entd_file_t));
            g_encounter_set = entd_set;
            lba = DISC_SECTOR_BATTLE_ENTD3_ENT;
        } else {
            entd_set = heap_alloc(sizeof(entd_file_t));
            g_encounter_set = entd_set;
            lba = DISC_SECTOR_BATTLE_ENTD4_ENT;
        }

        if (file_request_read_bytes(lba, sizeof(entd_file_t), entd_set) != 0) {
            heap_call_free(g_encounter_set);
            return 0;
        }
    }
    return 1;
}

/* Return the current event's encounter in the loaded ENTD file: -1 when there
 * is no current event, NULL while the file is still loading. Event ids 0-127
 * index the file directly; each further range of 128 ids (the last one
 * open-ended) indexes it from its own base. */
entd_encounter_t* encounter_get_current(void) {
    int* event_id = &g_encounter_current_event_id;
    int id;

    if (*event_id == 0) {
        return (entd_encounter_t*)-1;
    }
    if (file_is_still_loading() == 0) {
        id = *event_id;
        if (id < ENTD2_FIRST_EVENT) {
            return &g_encounter_set->encounters[id];
        }
        return &g_encounter_set->encounters[id < ENTD3_FIRST_EVENT ? id - ENTD2_FIRST_EVENT
                : id < ENTD4_FIRST_EVENT                           ? id - ENTD3_FIRST_EVENT
                                                                   : id - ENTD4_FIRST_EVENT];
    }
    return 0;
}

/* Rebuild the party roster from an event's ENTD: load the ENTD file that holds
 * event_id (blocking), empty the roster, save each of the encounter's 16 unit
 * slots into it through party_save_encounter_unit, and free the file. */
void encounter_init_event_units(int event_id) {
    entd_encounter_t* entd;
    int unit_id;

    g_encounter_current_event_id = event_id;
    while (encounter_open_file() == 0) {
        VSync(0);
        file_poll_load(&g_file_cd_state);
    }

    for (;;) {
        entd = encounter_get_current();
        if (entd == (entd_encounter_t*)-1) {
            break;
        }
        if (entd != 0) {
            party_clear();
            for (unit_id = 0; unit_id < ENTD_UNITS_PER_ENCOUNTER; unit_id++) {
                party_save_encounter_unit(entd, unit_id, 0, 1);
            }
            heap_call_free(g_encounter_set);
            break;
        }
        VSync(0);
        file_poll_load(&g_file_cd_state);
    }
}

/* Empty the inventory, then stock the new-game consumables: five Potions, two each of Hi-Potion,
 * Eye Drop and Phoenix Down, and one of each other basic restorative. */
void item_init_new_game_inventory(void) {
    s32 remaining;
    u8* quantity;

    for (remaining = ITEM_ID_COUNT - 1, quantity = &g_item_quantities[ITEM_ID_COUNT - 1]; remaining >= 0;
        remaining--, quantity--) {
        *quantity = 0;
    }

    g_item_quantities[ITEM_ID_POTION] = 5;
    g_item_quantities[ITEM_ID_HI_POTION] = 2;
    g_item_quantities[ITEM_ID_X_POTION] = 1;
    g_item_quantities[ITEM_ID_ETHER] = 1;
    g_item_quantities[ITEM_ID_HI_ETHER] = 1;
    g_item_quantities[ITEM_ID_ELIXIR] = 1;
    g_item_quantities[ITEM_ID_ANTIDOTE] = 1;
    g_item_quantities[ITEM_ID_EYE_DROP] = 2;
    g_item_quantities[ITEM_ID_ECHO_GRASS] = 1;
    g_item_quantities[ITEM_ID_MAIDENS_KISS] = 1;
    g_item_quantities[ITEM_ID_SOFT] = 1;
    g_item_quantities[ITEM_ID_HOLY_WATER] = 1;
    g_item_quantities[ITEM_ID_REMEDY] = 1;
    g_item_quantities[ITEM_ID_PHOENIX_DOWN] = 2;
}

enum {
    NEW_GAME_GENERATE_DEFAULT_UNIT = 0,
    NEW_GAME_LOAD_EVENT_PARTY = 2,
    STARTING_EVENT_ID = 254,
    STARTING_WAR_FUNDS = 2000,
};

/* Start a new roster: empty it, then generate Ramza alone (mode 0) or load the
 * party of event 254 from its ENTD (mode 2); any other mode leaves it empty. The
 * inventory is reset to the new-game stock and the war funds to 2000. */
void party_init_new_game(int mode) {
    screen_init_with_world_text();
    party_clear();

    if (mode == NEW_GAME_GENERATE_DEFAULT_UNIT) {
        party_generate_unit(UNIT_TYPE_RAMZA);
    } else if (mode == NEW_GAME_LOAD_EVENT_PARTY) {
        encounter_init_event_units(STARTING_EVENT_ID);
    }

    item_init_new_game_inventory();
    script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS, STARTING_WAR_FUNDS);
}

/* Store x, y and z in an SVECTOR, truncated to 16 bits. */
void vector_set_svector(SVECTOR* vector, s32 x, s32 y, s32 z) {
    vector->vx = x;
    vector->vy = y;
    vector->vz = z;
}

/* Store x, y and z in a VECTOR. */
void vector_set_vector(VECTOR* vector, s32 x, s32 y, s32 z) {
    vector->vx = x;
    vector->vy = y;
    vector->vz = z;
}
