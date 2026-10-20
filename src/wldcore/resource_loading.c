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
static s32 g_picture_sector_offsets[93] = {
    0,
    5,
    10,
    15,
    20,
    25,
    30,
    35,
    40,
    45,
    50,
    55,
    60,
    65,
    70,
    75,
    80,
    85,
    90,
    95,
    100,
    105,
    110,
    115,
    120,
    125,
    130,
    135,
    140,
    145,
    150,
    155,
    160,
    165,
    169,
    174,
    179,
    184,
    189,
    193,
    197,
    202,
    206,
    211,
    213,
    217,
    221,
    225,
    229,
    233,
    237,
    241,
    245,
    250,
    254,
    258,
    263,
    268,
    273,
    277,
    282,
    287,
    292,
    296,
    301,
    305,
    309,
    313,
    318,
    322,
    326,
    329,
    334,
    338,
    342,
    346,
    350,
    353,
    357,
    362,
    367,
    372,
    377,
    382,
    387,
    392,
    397,
    402,
    407,
    412,
    417,
    422,
    427,
};

static s32 g_picture_background_sector_offsets[132] = {
    0 * 60,
    1 * 60,
    2 * 60,
    3 * 60,
    4 * 60,
    5 * 60,
    6 * 60,
    7 * 60,
    8 * 60,
    9 * 60,
    10 * 60,
    11 * 60,
    12 * 60,
    13 * 60,
    14 * 60,
    15 * 60,
    16 * 60,
    17 * 60,
    18 * 60,
    19 * 60,
    20 * 60,
    21 * 60,
    22 * 60,
    23 * 60,
    24 * 60,
    25 * 60,
    26 * 60,
    27 * 60,
    28 * 60,
    29 * 60,
    30 * 60,
    31 * 60,
    32 * 60,
    33 * 60,
    34 * 60,
    35 * 60,
    36 * 60,
    37 * 60,
    38 * 60,
    39 * 60,
    40 * 60,
    41 * 60,
    42 * 60,
    43 * 60,
    44 * 60,
    45 * 60,
    46 * 60,
    47 * 60,
    48 * 60,
    49 * 60,
    50 * 60,
    51 * 60,
    52 * 60,
    53 * 60,
    54 * 60,
    55 * 60,
    56 * 60,
    57 * 60,
    58 * 60,
    59 * 60,
    60 * 60,
    61 * 60,
    62 * 60,
    63 * 60,
    64 * 60,
    65 * 60,
    66 * 60,
    67 * 60,
    68 * 60,
    69 * 60,
    70 * 60,
    71 * 60,
    72 * 60,
    73 * 60,
    74 * 60,
    75 * 60,
    76 * 60,
    77 * 60,
    78 * 60,
    79 * 60,
    80 * 60,
    81 * 60,
    82 * 60,
    83 * 60,
    84 * 60,
    85 * 60,
    86 * 60,
    87 * 60,
    88 * 60,
    89 * 60,
    90 * 60,
    91 * 60,
    92 * 60,
    93 * 60,
    94 * 60,
    95 * 60,
    96 * 60,
    97 * 60,
    98 * 60,
    99 * 60,
    100 * 60,
    101 * 60,
    102 * 60,
    103 * 60,
    104 * 60,
    105 * 60,
    106 * 60,
    107 * 60,
    108 * 60,
    109 * 60,
    110 * 60,
    111 * 60,
    112 * 60,
    113 * 60,
    114 * 60,
    115 * 60,
    116 * 60,
    117 * 60,
    118 * 60,
    119 * 60,
    120 * 60,
    121 * 60,
    122 * 60,
    123 * 60,
    124 * 60,
    125 * 60,
    126 * 60,
    127 * 60,
    128 * 60,
    129 * 60,
    130 * 60,
    131 * 60,
};

static s32 g_message_block_sector_offsets[113] = {
    0 * 22,
    1 * 22,
    2 * 22,
    3 * 22,
    4 * 22,
    5 * 22,
    6 * 22,
    7 * 22,
    8 * 22,
    9 * 22,
    10 * 22,
    11 * 22,
    12 * 22,
    13 * 22,
    14 * 22,
    15 * 22,
    16 * 22,
    17 * 22,
    18 * 22,
    19 * 22,
    20 * 22,
    21 * 22,
    22 * 22,
    23 * 22,
    24 * 22,
    25 * 22,
    26 * 22,
    27 * 22,
    28 * 22,
    29 * 22,
    30 * 22,
    31 * 22,
    32 * 22,
    33 * 22,
    34 * 22,
    35 * 22,
    36 * 22,
    37 * 22,
    38 * 22,
    39 * 22,
    40 * 22,
    41 * 22,
    42 * 22,
    43 * 22,
    44 * 22,
    45 * 22,
    46 * 22,
    47 * 22,
    48 * 22,
    49 * 22,
    50 * 22,
    51 * 22,
    52 * 22,
    53 * 22,
    54 * 22,
    55 * 22,
    56 * 22,
    57 * 22,
    58 * 22,
    59 * 22,
    60 * 22,
    61 * 22,
    62 * 22,
    63 * 22,
    64 * 22,
    65 * 22,
    66 * 22,
    67 * 22,
    68 * 22,
    69 * 22,
    70 * 22,
    71 * 22,
    72 * 22,
    73 * 22,
    74 * 22,
    75 * 22,
    76 * 22,
    77 * 22,
    78 * 22,
    79 * 22,
    80 * 22,
    81 * 22,
    82 * 22,
    83 * 22,
    84 * 22,
    85 * 22,
    86 * 22,
    87 * 22,
    88 * 22,
    89 * 22,
    90 * 22,
    91 * 22,
    92 * 22,
    93 * 22,
    94 * 22,
    95 * 22,
    96 * 22,
    97 * 22,
    98 * 22,
    99 * 22,
    100 * 22,
    101 * 22,
    102 * 22,
    103 * 22,
    104 * 22,
    105 * 22,
    106 * 22,
    107 * 22,
    108 * 22,
    109 * 22,
    110 * 22,
    111 * 22,
    112 * 22,
};

static s32 g_sound_novel_script_sector_offsets[7] = {
    0 * 6,
    1 * 6,
    2 * 6,
    3 * 6,
    4 * 6,
    5 * 6,
    6 * 6,
};

static s32 g_sound_novel_text_sector_offsets[7] = {
    0 * 20,
    1 * 20,
    2 * 20,
    3 * 20,
    4 * 20,
    5 * 20,
    6 * 20,
};

/* One step of the CD stream that loads an image file straight into VRAM (the
 * map texture WLDTEX.TM2): set double speed, seek, then read the sectors into
 * the two 2048-byte halves of `destination` in turn. Each sector holds a
 * count of rectangles, each a packed position, a packed size and its pixels,
 * uploaded with LoadImage. A disc error or a stall restarts from the speed
 * change at the current sector.
 *
 * The switch's jump table is the first 32 bytes of WLDCORE.BIN. */
void world_map_texture_stream_update(file_load_descriptor_t* stream) {
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
            file_cd_subsystems_reset();
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
s32 location_picture_load(s32 index) {
    s32 value = g_location_records[index].picture;

    if (value != 0)
        picture_load_set(value - 1);
    return value;
}

/* Load picture set `set` into its image buffer, skipping the transfer when it
 * is already resident. */
void picture_load_set(s32 set) {
    if (set != g_picture_loaded_set) {
        file_pending_wait();
        g_picture_loaded_set = set;
        file_pending_read_request(&g_file_cd_state, g_picture_sector_offsets[set] + DISC_SECTOR_WORLD_WLDPIC_BIN,
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
        file_pending_wait();
        g_picture_background_loaded_set = set;
        file_pending_read_request(&g_file_cd_state,
            g_picture_background_sector_offsets[set] + DISC_SECTOR_WORLD_WLDBK_BIN,
            g_picture_background_sector_offsets[set + 1] - g_picture_background_sector_offsets[set],
            g_world_map_scratch_buffer);
    }
}

/* Loads message block `block` (message_block_e) unless it is the one already loaded.
 * g_message_block_sector_offsets has the same shape as the pair used by
 * sound_novel_files_load: entry i is the start sector of block i relative to the disc
 * LBA below, so entry i+1 minus entry i is that block's length. */
void message_block_load_if_changed(s32 block) {
    g_text_section_pointers[TEXT_SECTION_MESSAGES] = g_message_block_buffer;
    if (block != g_message_block_loaded) {
        file_pending_wait();
        g_message_block_loaded = block;
        file_pending_read_request(&g_file_cd_state,
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
void sound_novel_files_load(s32 chapter) {
    file_pending_wait();
    file_pending_read_request(&g_file_cd_state,
        g_sound_novel_script_sector_offsets[chapter] + DISC_SECTOR_WORLD_SNPLBIN_BIN,
        g_sound_novel_script_sector_offsets[chapter + 1] - g_sound_novel_script_sector_offsets[chapter],
        &g_sound_novel_script);
    file_pending_wait();
    file_pending_read_request(&g_file_cd_state,
        g_sound_novel_text_sector_offsets[chapter] + DISC_SECTOR_WORLD_SNPLMES_BIN,
        g_sound_novel_text_sector_offsets[chapter + 1] - g_sound_novel_text_sector_offsets[chapter],
        g_message_block_buffer);
    g_text_section_pointers[TEXT_SECTION_MESSAGES] = g_message_block_buffer;
    g_message_block_loaded = -1;
}

/* Queue a one-sector read of BATTLE.BIN's first sector with no destination,
 * which seeks the drive there while the event transition plays. */
void event_transition_seek_battle_bin(void) {
    file_pending_read_request(&g_file_cd_state, g_file_battle_sector, 1, 0);
}

/* Fill the rectangle with the 256x238 display area of frame
 * buffer 0 (y 0) or 1 (y 0xF0). */
void world_map_display_rect_set(s32 buffer_index, RECT* rect) {
    rect->w = 0x100;
    rect->x = 0;
    rect->h = 0xEE;
    if (buffer_index == 0) {
        rect->y = 0;
    } else {
        rect->y = 0xF0;
    }
}
