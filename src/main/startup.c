/* CD-ROM and CPU RAM reset, the zodiac frame, startup and game state reset, ENTD files and encounters, new game
 * inventory and party, and SVECTOR setting. */

#include "fft/data.h"
#include "fft/main.h"
#include "fft/script.h"
#include "fft/unit.h"
#include "fft/world_menu.h"
#include "psx/libapi.h"
#include "psx/libc.h"
#include "psx/libcd.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/libspu.h"
#include "psx/types.h"

static void main_file_reset_cdrom_cpu_ram(void) {
    SetMem(2);
    CdInit();
    CdSetDebug(0);
    main_file_reset_pause_cdrom(&g_main_file_cd_state);
    g_main_file_still_loading = 0;
}

/* BATTLE/ZODIAC.BIN is a headerless image, not an executable overlay.
 * Sector 0xec61 supplies 0x10000 bytes: 128 * 256 16-bit VRAM transfer
 * values at (640, 0), subsequently rendered as 8-bit indexed texture data. */
void main_gfx_load_zodiac_frame(void) {
    RECT destination = { 640, 0, 128, 256 };
    u32* image;

    image = main_file_load_checked_to_heap(DISC_SECTOR_BATTLE_ZODIAC_BIN, 0x10000);
    LoadImage(&destination, image);
    DrawSync(0);
    main_heap_free(image);
    main_gfx_load_frame_bin_into_vram();
}

/* Power-on initialization: install the VSync, DrawSync and CD callbacks, start
 * the GPU, pad, SPU and CD, show the two logos, open the memory-card events and
 * load the shared sounds and the zodiac frame while the Squaresoft logo is up.
 *
 * rand is seeded with 1, and g_main_boot_startup2_has_run is cleared so the first
 * main_boot_reset_game_state does not repeat the hardware restart. */
void main_boot_run_startup(void) {
    main_heap_clear_game_allocator_table();
    ResetCallback();

    g_main_system_play_time_hours = 0;
    g_main_system_play_time_minutes = 0;
    g_main_system_play_time_seconds = 0;
    g_main_system_play_time_frames = 0;

    VSyncCallback(main_system_handle_vsync_callback);
    DrawSyncCallback(main_system_handle_draw_sync_callback);
    CdReadyCallback(main_file_handle_cd_ready_callback);
    CdReadCallback(main_file_handle_cd_read_callback);

    ResetGraph(0);
    SetGraphDebug(0);
    PadInit(0);
    SpuInit();
    main_file_reset_cdrom_cpu_ram();
    main_gfx_reset_display(256, 240, 512, 0, 0, 0);
    main_boot_build_and_draw_sceap_logo();
    main_boot_build_and_draw_squaresoft_logo();
    main_card_init_events();
    main_sound_open_generic_sfx();
    main_gfx_load_zodiac_frame();
    srand(1);
    main_boot_fade_out_squaresoft_logo();
    g_main_boot_startup2_has_run = 0;
}

/* Prepare a fresh session before the opening runs: reset the 32-bit save values and
 * options, rebuild the status check sets and restore the save data and item order
 * tables.
 *
 * On the first pass after power-on only that is done, because
 * main_boot_run_startup has just shown the logos. On every later pass (a soft
 * reset or the end of the game) it first frees the game heap, unloads the
 * scenario music, zeroes the play time, restarts the GPU, SPU and CD, and shows
 * the Squaresoft logo again for at least 60 frames while the shared sounds and
 * the zodiac frame reload. */
void main_boot_reset_game_state(void) {
    int frame;

    if (g_main_boot_startup2_has_run != 0) {
        main_heap_clear_game_allocator_table();
        main_sound_unload_scenario_music_and_tunes();
        g_main_system_play_time_hours = 0;
        g_main_system_play_time_minutes = 0;
        g_main_system_play_time_seconds = 0;
        g_main_system_play_time_frames = 0;
        ResetGraph(1);
        SetGraphDebug(0);
        SpuInitHot();
        main_file_reset_cdrom_cpu_ram();
        main_gfx_reset_display(256, 240, 512, 0, 0, 0);
        main_boot_build_and_draw_squaresoft_logo();
        main_sound_open_generic_sfx();
        main_gfx_load_zodiac_frame();
    }

    main_save_init_state_and_options();
    main_status_init_check_data();
    main_save_init_data_tables();
    main_item_init_order_tables();

    if (g_main_boot_startup2_has_run != 0) {
        for (frame = 0; frame < 60; frame++) {
            VSync(0);
        }
        main_boot_fade_out_squaresoft_logo();
    }

    g_main_boot_startup2_has_run = 1;
}

/* Start loading the ENTD file that holds g_main_entd_current_event_id into a new
 * 0x14000-byte heap block (g_main_entd_set). The four files cover 128 events
 * each.
 *
 * Returns 1 once the read is queued, and also for event 0, which loads nothing.
 * Returns 0 with the block freed when the loader is busy; the caller retries. */
int main_entd_open_file(void) {
    int lba;
    entd_file_t* entd_set;

    if (g_main_entd_current_event_id != 0) {
        if (g_main_entd_current_event_id < ENTD2_FIRST_EVENT) {
            entd_set = main_heap_alloc(sizeof(entd_file_t));
            g_main_entd_set = entd_set;
            lba = DISC_SECTOR_BATTLE_ENTD1_ENT;
        } else if (g_main_entd_current_event_id < ENTD3_FIRST_EVENT) {
            entd_set = main_heap_alloc(sizeof(entd_file_t));
            g_main_entd_set = entd_set;
            lba = DISC_SECTOR_BATTLE_ENTD2_ENT;
        } else if (g_main_entd_current_event_id < ENTD4_FIRST_EVENT) {
            entd_set = main_heap_alloc(sizeof(entd_file_t));
            g_main_entd_set = entd_set;
            lba = DISC_SECTOR_BATTLE_ENTD3_ENT;
        } else {
            entd_set = main_heap_alloc(sizeof(entd_file_t));
            g_main_entd_set = entd_set;
            lba = DISC_SECTOR_BATTLE_ENTD4_ENT;
        }

        if (main_file_call_build_header(lba, sizeof(entd_file_t), entd_set) != 0) {
            main_heap_call_free(g_main_entd_set);
            return 0;
        }
    }
    return 1;
}

/* Returns -1 for no current encounter and NULL while its file is loading.
 * Event ids 0-127 index the loaded ENTD file directly; each further
 * range of 128 ids (the last one open-ended) indexes it from its own base. */
entd_encounter_t* main_entd_get_encounter(void) {
    int* event_id = &g_main_entd_current_event_id;
    int id;

    if (*event_id == 0) {
        return (entd_encounter_t*)-1;
    }
    if (main_file_is_still_loading() == 0) {
        id = *event_id;
        if (id < ENTD2_FIRST_EVENT) {
            return &g_main_entd_set->encounters[id];
        }
        return &g_main_entd_set->encounters[id < ENTD3_FIRST_EVENT ? id - ENTD2_FIRST_EVENT
                : id < ENTD4_FIRST_EVENT                           ? id - ENTD3_FIRST_EVENT
                                                                   : id - ENTD4_FIRST_EVENT];
    }
    return 0;
}

/* Rebuild the party roster from an event's ENTD: load the ENTD file that holds
 * event_id (blocking), empty the roster, save each of the encounter's 16 unit
 * slots into it through main_unit_init, and free the file. */
void main_entd_init_event_unit_data(int event_id) {
    entd_encounter_t* entd;
    int unit_id;

    g_main_entd_current_event_id = event_id;
    while (main_entd_open_file() == 0) {
        VSync(0);
        main_file_poll_load(&g_main_file_cd_state);
    }

    for (;;) {
        entd = main_entd_get_encounter();
        if (entd == (entd_encounter_t*)-1) {
            break;
        }
        if (entd != 0) {
            main_party_clear_all();
            for (unit_id = 0; unit_id < ENTD_UNITS_PER_ENCOUNTER; unit_id++) {
                main_unit_init(entd, unit_id, 0, 1);
            }
            main_heap_call_free(g_main_entd_set);
            break;
        }
        VSync(0);
        main_file_poll_load(&g_main_file_cd_state);
    }
}

void main_item_init_new_game_inventory(void) {
    s32 remaining;
    u8* quantity;

    for (remaining = ITEM_ID_COUNT - 1, quantity = &g_main_item_quantities[ITEM_ID_COUNT - 1]; remaining >= 0;
        remaining--, quantity--) {
        *quantity = 0;
    }

    g_main_item_quantities[ITEM_ID_POTION] = 5;
    g_main_item_quantities[ITEM_ID_HI_POTION] = 2;
    g_main_item_quantities[ITEM_ID_X_POTION] = 1;
    g_main_item_quantities[ITEM_ID_ETHER] = 1;
    g_main_item_quantities[ITEM_ID_HI_ETHER] = 1;
    g_main_item_quantities[ITEM_ID_ELIXIR] = 1;
    g_main_item_quantities[ITEM_ID_ANTIDOTE] = 1;
    g_main_item_quantities[ITEM_ID_EYE_DROP] = 2;
    g_main_item_quantities[ITEM_ID_ECHO_GRASS] = 1;
    g_main_item_quantities[ITEM_ID_MAIDENS_KISS] = 1;
    g_main_item_quantities[ITEM_ID_SOFT] = 1;
    g_main_item_quantities[ITEM_ID_HOLY_WATER] = 1;
    g_main_item_quantities[ITEM_ID_REMEDY] = 1;
    g_main_item_quantities[ITEM_ID_PHOENIX_DOWN] = 2;
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
void main_party_init_new_game(int mode) {
    world_menu_load_text_1bd8();
    main_party_clear_all();

    if (mode == NEW_GAME_GENERATE_DEFAULT_UNIT) {
        main_party_generate_unit(MAIN_UNIT_TYPE_RAMZA);
    } else if (mode == NEW_GAME_LOAD_EVENT_PARTY) {
        main_entd_init_event_unit_data(STARTING_EVENT_ID);
    }

    main_item_init_new_game_inventory();
    script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS, STARTING_WAR_FUNDS);
}

void main_util_set_svector(SVECTOR* vector, s32 x, s32 y, s32 z) {
    vector->vx = x;
    vector->vy = y;
    vector->vz = z;
}

void main_util_set_vector(VECTOR* vector, s32 x, s32 y, s32 z) {
    vector->vx = x;
    vector->vy = y;
    vector->vz = z;
}
