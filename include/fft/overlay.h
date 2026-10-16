#ifndef OVERLAY_H
#define OVERLAY_H

/* Loading the BIN programs from MAIN, and the companion EVENT overlays that BATTLE and WORLD load beside the scene:
 * their requests, load tables and the per-frame drivers. */

#include "psx/libcd.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* programs */
int overlay_exec_open_title(int mode);
void overlay_exec_open_ending(void);
int overlay_open_world_and_wldcore(int load_world);
void overlay_open_world(s32 mode);

/* companion overlays */
/* Companion overlay requests: index into the overlay LBA, size and load offset
 * tables (g_overlay_companion_executable_lbas). */
typedef enum overlay_companion {
    COMPANION_OVERLAY_OPTION = 1,             /* EVENT/OPTION.OUT */
    COMPANION_OVERLAY_BUNIT = 2,              /* EVENT/BUNIT.OUT, first half */
    COMPANION_OVERLAY_HELPMENU = 3,           /* EVENT/HELPMENU.OUT */
    COMPANION_OVERLAY_EVTOOL = 4,             /* EVENT/EVTOOL.OUT; no request in retail */
    COMPANION_OVERLAY_DEBUGMAP = 5,           /* EVENT/DEBUGMAP.OUT; no request in retail */
    COMPANION_OVERLAY_REQUIRE = 6,            /* EVENT/REQUIRE.OUT */
    COMPANION_OVERLAY_JOBSTTS = 7,            /* EVENT/JOBSTTS.OUT */
    COMPANION_OVERLAY_SMALL = 8,              /* EVENT/SMALL.OUT */
    COMPANION_OVERLAY_EQUIP = 9,              /* EVENT/EQUIP.OUT, first half */
    COMPANION_OVERLAY_EQUIP_SECOND_HALF = 10, /* loaded above the heap reservation */
    COMPANION_OVERLAY_DEBUGCHR = 11,          /* EVENT/DEBUGCHR.OUT */
    COMPANION_OVERLAY_ETC = 12,               /* EVENT/ETC.OUT */
    COMPANION_OVERLAY_ATTACK = 13,            /* EVENT/ATTACK.OUT */
    COMPANION_OVERLAY_CARD = 14,              /* EVENT/CARD.OUT */
    COMPANION_OVERLAY_BUNIT_SECOND_HALF = 15, /* loaded above the heap reservation */
} overlay_companion_e;

/* Companion data file requests (the second LBA and size tables). */
typedef enum overlay_companion_file {
    COMPANION_FILE_WIN001 = 2,    /* EVENT/WIN001.BIN */
    COMPANION_FILE_BONUS = 3,     /* EVENT/BONUS.BIN */
    COMPANION_FILE_SPELL_MES = 4, /* EVENT/SPELL.MES */
} overlay_companion_file_e;

/* g_overlay_companion_overlay_state: the overlay entry the frame driver calls, cleared when the overlay
 * returns. */
typedef enum overlay_companion_state {
    COMPANION_OVERLAY_STATE_IDLE = 0,
    COMPANION_OVERLAY_STATE_STUB = 1,      /* an empty function */
    COMPANION_OVERLAY_STATE_BUNIT = 2,     /* unit_list_entrypoint */
    COMPANION_OVERLAY_STATE_JOBSTTS = 3,   /* status_screen_entrypoint */
    COMPANION_OVERLAY_STATE_EQUIP = 4,     /* equipment_entrypoint */
    COMPANION_OVERLAY_STATE_CARD_SAVE = 5, /* save_run_menu */
} overlay_companion_state_e;

extern s32 g_overlay_companion_overlay_state;
extern s32 g_overlay_companion_overlay_primitives;
extern s32 g_overlay_companion_executable_request_index;
extern s32 g_overlay_companion_executable_loading;
extern s32 g_overlay_companion_executable_lbas[];
extern s32 g_overlay_companion_executable_sizes[];
extern s32 g_overlay_companion_executable_offsets[];
extern s32 g_overlay_companion_data_file_request_index;
extern s32 g_overlay_companion_data_file_loading;
extern s32 g_overlay_companion_data_file_lbas[];
extern s32 g_overlay_companion_data_file_sizes[];
extern u8* g_overlay_companion_data_file_buffer;

/* Publishes an executable request and waits for it to clear. */
void overlay_request_companion(s32 request_index);

/* The per-frame drivers of the executable and data file requests: they start the disc read on the main stack,
 * poll until it finishes and clear the request. */
void overlay_poll_companion(void);
void overlay_poll_companion_file(void);

/* Requests SMALL.OUT without waiting; overlay_poll_small drives the load during map initialization and returns
 * 1 while it is pending. */
void overlay_request_small(void);
s32 overlay_poll_small(void);

/* The overlay entry of COMPANION_OVERLAY_STATE_STUB. */
void overlay_companion_stub(void);

#endif
