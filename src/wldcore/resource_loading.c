/* Disc resources for world-map screens: stream the map texture into VRAM and
 * cache picture sets, backgrounds, message blocks and sound-novel chapters.
 * Frame-buffer rectangles support the screens that display those resources. */

#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/location.h"
#include "fft/message.h"
#include "fft/screen.h"
#include "fft/sound_novel.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/world_map.h"
#include "psx/libcd.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

extern s32 g_file_battle_sector;
extern u8 g_message_block_buffer[];

/* File-sector boundaries, including one terminal offset after the last payload. */
extern s32 g_picture_sector_offsets[93];
extern s32 g_picture_background_sector_offsets[132];
extern s32 g_message_block_sector_offsets[113];
extern s32 g_sound_novel_script_sector_offsets[7];
extern s32 g_sound_novel_text_sector_offsets[7];

/* One step of the CD stream that loads an image file straight into VRAM (the
 * map texture WLDTEX.TM2): set double speed, seek, then read the sectors into
 * the two 2048-byte halves of `destination` in turn. Each sector holds a
 * count of rectangles, each a packed position, a packed size and its pixels,
 * uploaded with LoadImage. A disc error or a stall restarts from the speed
 * change at the current sector.
 *
 * The switch's jump table is the first 32 bytes of WLDCORE.BIN. */
void world_map_poll_texture_stream(file_load_descriptor_t* stream) {
    RECT rect;
    u8 mode;
    s32 rect_count;
    s32 word;
    s32 n;
    s32 status;

    switch (stream->state) {
    case FILE_LOAD_STATE_IDLE:
        break;
    case FILE_LOAD_STATE_SET_DOUBLE_SPEED:
        mode = CdlModeSpeed;
        CdControl(CdlSetmode, &mode, 0);
        g_system_wait_frames = 0;
        stream->state++;
        break;
    case FILE_LOAD_STATE_WAIT_AFTER_SET_MODE:
        if (g_system_wait_frames < 4) {
            return;
        }
        stream->state++;
        break;
    case FILE_LOAD_STATE_START_SEEK:
        CdControlF(CdlSetloc, (u8*)&stream->position);
        stream->state++;
        break;
    case FILE_LOAD_STATE_POLL_SEEK:
        status = CdSync(1, 0);
        if (status == CdlComplete) {
            stream->state++;
            break;
        }
        if (status != CdlDiskError) {
            return;
        }
        stream->state = FILE_LOAD_STATE_SET_DOUBLE_SPEED;
        stream->error_count++;
        return;
    case FILE_LOAD_STATE_START_READ:
        if (CdRead2(CdlModeSpeed) == 0) {
            stream->state = FILE_LOAD_STATE_SET_DOUBLE_SPEED;
            stream->error_count++;
            return;
        }
        stream->wait_frames = 0;
        stream->sector_index = 0;
        stream->state++;
        break;
    case FILE_LOAD_STATE_POLL_READ:
        status = CdReady(1, 0);
        if (status == CdlDataReady) {
            CdGetSector(stream->destination + (stream->sector_index & 1) * PSYQ_CD_DATA_WORDS, PSYQ_CD_DATA_WORDS);
            stream->wait_frames = 0;
            stream->state++;
            break;
        }
        if (status == CdlDiskError) {
            stream->state = FILE_LOAD_STATE_SET_DOUBLE_SPEED;
            stream->error_count++;
        }
        if (stream->wait_frames > 0x40000 || (CdStatus() & 1) != 0) {
            file_reset_cd_subsystems();
            CdIntToPos(stream->lba + stream->sector_index, &stream->position);
            stream->state = FILE_LOAD_STATE_SET_DOUBLE_SPEED;
            stream->error_count++;
            return;
        }
        stream->wait_frames++;
        return;
    case FILE_LOAD_STATE_UPLOAD_VRAM:
        if (CdDataSync(1) != 0) {
            return;
        }
        /* Each sector holds a rectangle count, then per rectangle a packed
         * position, a packed size and the pixels. Keep the original word cursor:
         * a typed rectangle cursor folds address calculations and shortens the body. */
        word = (stream->sector_index & 1) * PSYQ_CD_DATA_WORDS;
        rect_count = stream->destination[(stream->sector_index & 1) * PSYQ_CD_DATA_WORDS];
        word++;
        for (n = 0; n < rect_count; n++) {
            rect.x = stream->destination[word];
            rect.y = (stream->destination[word] & 0xFFFF0000) >> 16;
            word++;
            rect.w = stream->destination[word];
            rect.h = (stream->destination[word] & 0xFFFF0000) >> 16;
            word++;
            LoadImage(&rect, stream->destination + word);
            word += rect.w * rect.h / 2;
        }
        stream->sector_index++;
        if (stream->sector_index < stream->sector_count) {
            stream->state = FILE_LOAD_STATE_POLL_READ;
            return;
        }
        stream->state = FILE_LOAD_STATE_IDLE;
        CdControlB(CdlPause, 0, 0);
        return;
    }
}

/* Load the picture data set of location record `index` (stored as set + 1)
 * and return the stored value; 0 means the record has no picture. */
s32 location_load_picture(s32 index) {
    s32 value = g_location_records[index].picture;

    if (value != 0)
        picture_load_set(value - 1);
    return value;
}

/* Load picture set `set` into its image buffer, skipping the transfer when it
 * is already resident. */
void picture_load_set(s32 set) {
    if (set != g_picture_loaded_set) {
        file_wait_for_pending();
        g_picture_loaded_set = set;
        file_wait_then_request_read(&g_file_cd_state, g_picture_sector_offsets[set] + DISC_SECTOR_WORLD_WLDPIC_BIN,
            g_picture_sector_offsets[set + 1] - g_picture_sector_offsets[set], g_picture_buffer);
    }
}

/* Load background set `set` of WLDBK.BIN into the shared scratch buffer,
 * skipping the transfer when it is already resident.
 *
 * Entry i of g_picture_background_sector_offsets is the start sector of set i
 * in the file, so entry i+1 minus entry i is that set's sector count; the
 * other sector-offset tables in this file have the same shape. */
void picture_load_background_set(s32 set) {
    if (set != g_picture_background_loaded_set) {
        file_wait_for_pending();
        g_picture_background_loaded_set = set;
        file_wait_then_request_read(&g_file_cd_state,
            g_picture_background_sector_offsets[set] + DISC_SECTOR_WORLD_WLDBK_BIN,
            g_picture_background_sector_offsets[set + 1] - g_picture_background_sector_offsets[set],
            g_world_map_scratch_buffer);
    }
}

/* Loads message block `block` (message_block_e) unless it is the one already loaded.
 * g_message_block_sector_offsets has the same shape as the pair used by
 * sound_novel_load_files: entry i is the start sector of block i relative to the disc
 * LBA below, so entry i+1 minus entry i is that block's length. */
void message_block_load_if_changed(s32 block) {
    g_text_section_pointers[TEXT_SECTION_MESSAGES] = g_message_block_buffer;
    if (block != g_message_block_loaded) {
        file_wait_for_pending();
        g_message_block_loaded = block;
        file_wait_then_request_read(&g_file_cd_state,
            g_message_block_sector_offsets[block] + DISC_SECTOR_WORLD_WLDMES_BIN,
            g_message_block_sector_offsets[block + 1] - g_message_block_sector_offsets[block], g_message_block_buffer);
    }
}

/* Load sound-novel chapter `chapter`: its script into the save slot buffer
 * and its text into the message buffer, which becomes the message section.
 *
 * Each file comes from a seven-entry sector-offset table. An entry is the
 * start sector of one chapter's data relative to the disc LBA added to it,
 * so entry i+1 minus entry i is that chapter's sector count. */
void sound_novel_load_files(s32 chapter) {
    file_wait_for_pending();
    file_wait_then_request_read(&g_file_cd_state,
        g_sound_novel_script_sector_offsets[chapter] + DISC_SECTOR_WORLD_SNPLBIN_BIN,
        g_sound_novel_script_sector_offsets[chapter + 1] - g_sound_novel_script_sector_offsets[chapter],
        &g_sound_novel_script);
    file_wait_for_pending();
    file_wait_then_request_read(&g_file_cd_state,
        g_sound_novel_text_sector_offsets[chapter] + DISC_SECTOR_WORLD_SNPLMES_BIN,
        g_sound_novel_text_sector_offsets[chapter + 1] - g_sound_novel_text_sector_offsets[chapter],
        g_message_block_buffer);
    g_text_section_pointers[TEXT_SECTION_MESSAGES] = g_message_block_buffer;
    g_message_block_loaded = -1;
}

/* Queue a one-sector read of BATTLE.BIN's first sector with no destination,
 * which seeks the drive there while the event transition plays. */
void event_transition_seek_battle_bin(void) {
    file_wait_then_request_read(&g_file_cd_state, g_file_battle_sector, 1, 0);
}

/* Fill the rectangle with the 256x238 display area of frame
 * buffer 0 (y 0) or 1 (y 0xF0). */
void world_map_set_display_rect(s32 buffer_index, RECT* rect) {
    rect->w = 0x100;
    rect->x = 0;
    rect->h = 0xEE;
    if (buffer_index == 0) {
        rect->y = 0;
    } else {
        rect->y = 0xF0;
    }
}
