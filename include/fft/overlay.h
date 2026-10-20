#ifndef OVERLAY_H
#define OVERLAY_H

/* Loading the BIN programs from MAIN, and the companion EVENT overlays that BATTLE and WORLD load beside the scene:
 * their requests, load tables and the per-frame drivers. */

#include "psx/types.h"

/* programs */
void overlay_ending_run(void);
void overlay_world_load(s32 mode);

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
    COMPANION_OVERLAY_BUNIT_SECOND_HALF = 15  /* loaded above the heap reservation */
} overlay_companion_e;

/* Companion data file requests (the second LBA and size tables). */
typedef enum overlay_companion_file {
    COMPANION_FILE_WIN001 = 2,   /* EVENT/WIN001.BIN */
    COMPANION_FILE_BONUS = 3,    /* EVENT/BONUS.BIN */
    COMPANION_FILE_SPELL_MES = 4 /* EVENT/SPELL.MES */
} overlay_companion_file_e;

/* g_overlay_companion_overlay_state: the overlay entry the frame driver calls, cleared when the overlay
 * returns. */
typedef enum overlay_companion_state {
    COMPANION_OVERLAY_STATE_IDLE = 0,
    COMPANION_OVERLAY_STATE_STUB = 1,     /* an empty function */
    COMPANION_OVERLAY_STATE_BUNIT = 2,    /* bunit_entrypoint */
    COMPANION_OVERLAY_STATE_JOBSTTS = 3,  /* jobstts_entrypoint */
    COMPANION_OVERLAY_STATE_EQUIP = 4,    /* equip_entrypoint */
    COMPANION_OVERLAY_STATE_CARD_SAVE = 5 /* card_entrypoint */
} overlay_companion_state_e;

extern s32 g_overlay_companion_overlay_state;
extern s32 g_overlay_companion_overlay_primitives;
extern s32 g_overlay_companion_executable_request_index;

/* Publishes an executable request and waits for it to clear. */
void overlay_companion_request(s32 request_index);

/* The per-frame drivers of the executable and data file requests: they start the disc read on the main stack,
 * poll until it finishes and clear the request. */
void overlay_companion_poll(void);
void overlay_companion_file_poll(void);

/* Requests SMALL.OUT without waiting; overlay_small_poll drives the load during map initialization and returns
 * 1 while it is pending. */
void overlay_small_request(void);
s32 overlay_small_poll(void);

/* The overlay entry of COMPANION_OVERLAY_STATE_STUB. */
void overlay_companion_stub(void);

#endif
