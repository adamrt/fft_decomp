/* Load WORLD and WLDCORE into their separate overlay regions, then enter the world map; their
 * blocking reads preserve or initialize the resident loading display. Then reset the save record
 * fields and the new-game data tables. Only the older ASPSX assembler reproduces the reset. */

#include "fft/sys/file.h"
#include "fft/sys/memory.h"
#include "fft/sys/overlay.h"
#include "fft/world/brave_story.h"
#include "fft/world/novel.h"
#include "fft/world/proposition.h"
#include "fft/world/save.h"
#include "fft/world/world_map.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/types.h"

static void load_disc_sectors(
    int sector, int sectors, void* destination, file_loading_display_mode_e loading_display_mode);

/* world overlays */

extern u8* g_heap_world_overlay_load_address;
static void load_disc_sectors(
    int sector, int sectors, void* destination, file_loading_display_mode_e loading_display_mode);

/* Load WLDCORE.BIN into the low overlay area with the Now Loading screen and, when load_world is
 * set, WORLD.BIN at the world overlay address, then run WLDCORE's entry point and return its
 * result. */
world_map_result_e overlay_run_world(s32 load_world) {
    load_disc_sectors(DISC_SECTOR_WORLD_WLDCORE_BIN, 0xDC, g_heap_low_overlay_load_address, FILE_LOADING_DISPLAY_INIT);
    if (load_world != 0) {
        load_disc_sectors(
            DISC_SECTOR_WORLD_WORLD_BIN, 0x1E0, g_heap_world_overlay_load_address, FILE_LOADING_DISPLAY_PRESERVE);
    }
    return wldcore_entrypoint();
}

/* Load WORLD.BIN alone. mode - 1 is the loading display mode of
 * file_request_read, so mode 1 shows the "Now Loading" screen. */
void overlay_load_world(s32 mode) {
    load_disc_sectors(DISC_SECTOR_WORLD_WORLD_BIN, 0x1E0, g_heap_world_overlay_load_address, mode - 1);
}

/* Blocking read of whole sectors to destination through the shared loader. The
 * last argument is file_request_read's loading display mode: 0 blanks the
 * screen and sets up the "Now Loading" message, 1 leaves the display state alone,
 * and -1 turns the message off. */
static void load_disc_sectors(
    int sector, int sectors, void* destination, file_loading_display_mode_e loading_display_mode) {
    file_request_read(&g_file_cd_state, sector, sectors, destination, loading_display_mode);
    while (g_file_cd_state.state != FILE_LOAD_STATE_IDLE) {
        file_poll_load(&g_file_cd_state);
        VSync(0);
    }
}

/* save tables */

novel_reading_t g_novel_saved_readings[5] = { 0 };

extern u8* g_brave_story_ages_source;

/* Reset the save record fields and new-game data tables: the five saved
 * sound-novel states (no text history, picture or background, zero counters
 * and flags, each on the shared save-slot buffer, brightness 0x10 except in
 * records 2 and 3), the Brave Story character ages from their template, the
 * saved data bits, the proposition states and count, and the system flags.
 *
 * Assembled with ASPSX < 2.30: the five stores that index
 * g_novel_saved_readings[i] instead of going through record are the target's
 * 4-instruction `lui $at / addiu $at / addu $at,idx / store 0($at)` forms, so
 * this function carries the per-function gcc-2.6.3_O2_aspsx-2.21 profile. */
void save_init_data_tables(void) {
    s32 i;
    novel_reading_t* record;

    for (i = 0; i < 5; i++) {
        record = &g_novel_saved_readings[i];
        record->text_history[3] = -1;
        record->text_history[2] = -1;
        record->text_history[1] = -1;
        record->text_history[0] = -1;
        record->picture_id = -1;
        g_novel_saved_readings[i].background_set = -1;
        record->sound_mode = NOVEL_SOUND_MODE_EFFECTS_ONLY;
        record->music_selection = 0;
        record->state_flags = 0;
        g_novel_saved_readings[i].section = 0;
        g_novel_saved_readings[i].counter = 0;
        record->_unread_18 = 0;
        record->text_id = 0;
        g_novel_saved_readings[i].counter_delta = 0;
        g_novel_saved_readings[i].buffer = &g_novel_script;
    }
    g_novel_saved_readings[0].brightness = 0x10;
    g_novel_saved_readings[1].brightness = 0x10;
    g_novel_saved_readings[2].brightness = 0;
    g_novel_saved_readings[3].brightness = 0;
    g_novel_saved_readings[4].brightness = 0x10;
    memcpy(g_brave_story_character_ages, g_brave_story_ages_source, sizeof(g_brave_story_character_ages));
    for (i = 0; i < 40; i++) {
        g_novel_read_section_bits[i] = 0;
    }
    for (i = 0; i < 2; i++) {
        g_novel_saved_flags[i] = 0;
    }
    for (i = 0; i < 96; i++) {
        g_proposition_states[i] = 0;
    }
    g_proposition_count = 0;
    g_world_map_flags = 0;
}
