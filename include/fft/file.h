#ifndef FILE_H
#define FILE_H

/* Disc files and the CD loader: the files read at fixed sectors, MAIN's resident asynchronous loader, the loading
 * helpers the overlays, OPEN and WLDCORE build around it, and OPEN's title background requests. */

#include "psx/libcd.h"
#include "psx/types.h"

struct suzuki_smd_header;

/* disc files */
/* Sector and byte count requested through the resident file loader.
 * The count includes any sector rounding needed by the resource. */
typedef struct file_extent {
    s32 sector;
    s32 size;
} file_extent_t;
typedef char file_extent_size_must_be_8[(sizeof(file_extent_t) == 8) ? 1 : -1];

/* First sector of each disc file the code reads at a fixed sector, named as
 * the disc directory names the file. The other files are found through
 * sector tables in the data. */
typedef enum disc_sector {
    DISC_SECTOR_SCEAP_DAT = 0xc6,
    DISC_SECTOR_BATTLE_BIN = 0x3e8,
    DISC_SECTOR_EVENT_ATTACK_OUT = 0x990,
    DISC_SECTOR_EVENT_MAPTITLE_BIN = 0xdac, /* five sectors for each four maps */
    DISC_SECTOR_EVENT_FRAME_BIN = 0xe68,
    DISC_SECTOR_EVENT_TEST_EVT = 0xe7b,     /* EVENT_BLOCK_SECTORS per event_id_e */
    DISC_SECTOR_EVENT_EVTFACE_BIN = 0x164b, /* four sectors per portrait */
    DISC_SECTOR_EVENT_UNIT_BIN = 0x166b,
    DISC_SECTOR_EVENT_BONUS_BIN = 0x16c0, /* thirteen sectors per image */
    DISC_SECTOR_EVENT_ITEM_BIN = 0x1899,
    DISC_SECTOR_EVENT_WLDFACE_BIN = 0x18ba,
    DISC_SECTOR_EVENT_ATCHELP_LZW = 0x1a3a,
    DISC_SECTOR_EVENT_WORLD_LZW = 0x1bd8,
    DISC_SECTOR_EVENT_OPEN_LZW = 0x1c18,
    DISC_SECTOR_EVENT_EVTCHR_BIN = 0x1d4c, /* fifteen sectors per block */
    DISC_SECTOR_BATTLE_EFC_FNT_TIM = 0xdea8,
    DISC_SECTOR_BATTLE_WEP_SPR = 0xdefa,
    DISC_SECTOR_BATTLE_OTHER_SPR = 0xdf24,
    DISC_SECTOR_BATTLE_ENTD1_ENT = 0xebc1,
    DISC_SECTOR_BATTLE_ENTD2_ENT = 0xebe9,
    DISC_SECTOR_BATTLE_ENTD3_ENT = 0xec11,
    DISC_SECTOR_BATTLE_ENTD4_ENT = 0xec39,
    DISC_SECTOR_BATTLE_ZODIAC_BIN = 0xec61,
    DISC_SECTOR_MENU_FFTSAVE_DAT = 0x11a7f,
    DISC_SECTOR_WORLD_WLDTEX_TM2 = 0x11d28,
    DISC_SECTOR_WORLD_WLDPIC_BIN = 0x11dae,  /* by g_picture_sector_offsets */
    DISC_SECTOR_WORLD_WLDMES_BIN = 0x11f59,  /* by g_message_block_sector_offsets */
    DISC_SECTOR_WORLD_SNPLBIN_BIN = 0x128f9, /* by g_sound_novel_script_sector_offsets */
    DISC_SECTOR_WORLD_SNPLMES_BIN = 0x1291d, /* by g_sound_novel_text_sector_offsets */
    DISC_SECTOR_WORLD_WLDBK_BIN = 0x12995,   /* by g_picture_background_sector_offsets */
    DISC_SECTOR_WORLD_WLDCORE_BIN = 0x14849,
    DISC_SECTOR_WORLD_WORLD_BIN = 0x14925,
    DISC_SECTOR_SOUND_ENV_SED = 0x14c08,
    DISC_SECTOR_SOUND_SYSTEM_SED = 0x14c0a,
    DISC_SECTOR_SOUND_WAVESET_WD = 0x14c0f,
    DISC_SECTOR_OPEN_OPEN_BIN = 0x14ff0,
    DISC_SECTOR_OPEN_OPNTEX_BIN = 0x1505d,
    DISC_SECTOR_OPEN_OPNBK_BIN = 0x15243, /* the title backgrounds, by sector offset */
    DISC_SECTOR_OPEN_FFTST_STR = 0x153d6,
    DISC_SECTOR_OPEN_FFTOP_STR = 0x17b60,
    DISC_SECTOR_OPEN_FFTPRE_STR = 0x1e038,
    DISC_SECTOR_OPEN_FFTEND_STR = 0x21908,
    DISC_SECTOR_OPEN_FFTPR_STR = 0x25160,
    DISC_SECTOR_OPEN_FFTUNIT_STR = 0x26ec0,
    DISC_SECTOR_OPEN_ENDING_XA = 0x2f551
} disc_sector_e;

/* resident CD loader */
/* Loading-display mode passed to the shared file-header builder. */
typedef enum file_loading_display_mode {
    FILE_LOADING_DISPLAY_OFF = -1,
    FILE_LOADING_DISPLAY_INIT = 0,
    FILE_LOADING_DISPLAY_PRESERVE = 1
} file_loading_display_mode_e;

/* One poll advances at most one phase; recoverable CD errors restart setup. */
typedef enum file_load_state {
    FILE_LOAD_STATE_IDLE = 0,
    FILE_LOAD_STATE_SET_DOUBLE_SPEED = 1,
    FILE_LOAD_STATE_WAIT_AFTER_SET_MODE = 2,
    FILE_LOAD_STATE_START_SEEK = 3,
    FILE_LOAD_STATE_POLL_SEEK = 4,
    FILE_LOAD_STATE_START_READ = 5,
    FILE_LOAD_STATE_POLL_READ = 6,
    FILE_LOAD_STATE_UPLOAD_VRAM = 7 /* WLDCORE image stream only */
} file_load_state_e;

/* Asynchronous CD read state advanced by file_poll_load. */
typedef struct file_load_descriptor {
    s32 _unknown_00;
    s32 state; /* file_load_state_e */
    s32 error_count;
    s32 wait_frames;
    s32 sector_count;
    s32 lba;
    s32 sector_index; /* 0x18; sector being read by world_map_poll_texture_stream */
    CdlLOC position;
    u32* destination; /* word-aligned CD transfer buffer */
} file_load_descriptor_t;
typedef char file_load_descriptor_size_must_be_36[sizeof(file_load_descriptor_t) == 36 ? 1 : -1];

extern file_load_descriptor_t g_file_cd_state;

int file_request_read(
    file_load_descriptor_t* state, int lba, int sector_count, void* destination, int loading_display_mode);

/* Forwards file_request_read's result. */
s32 file_request_read_quiet(file_load_descriptor_t* header, s32 sector, s32 sectors, void* destination);

/* Byte-count reads transfer size >> 11 whole sectors, without rounding up. */
s32 file_request_read_bytes(s32 sector, u32 size, void* destination);
void file_poll_load(file_load_descriptor_t* state);
int file_is_still_loading(void);
void file_reset_pause_cdrom(file_load_descriptor_t* state);
void file_reset_cd_subsystems(void);
void file_handle_cd_read(u8 event, u8* result);
void file_handle_cd_ready(u8 event, u8* result);

/* Blocking byte-count loads use the same whole-sector rule. Heap loads allocate
 * size bytes, even when a partial final sector would remain unread. */
void* file_load_to_address(int sector, u32 size, void* destination);
void* file_load_checked_to_address(int sector, int size, void* destination);
void* file_load_checked_to_heap(int sector, int size);
struct suzuki_smd_header* file_load_smd(int sector, u32 size);

/* loading helpers */
void file_load_sync(s32 lba, s32 size, void* destination);
void file_wait_then_request_read(file_load_descriptor_t* header, s32 sector, s32 sector_count, void* destination);
void file_wait_for_pending(void);

/* OPEN title backgrounds */
extern void* g_file_destination;
extern s32 g_file_current_openbk_image_id;

void file_request_openbk_image(s32 image_id, void* destination);
s32 file_get_cd_sync_state_delta(void);

#endif
