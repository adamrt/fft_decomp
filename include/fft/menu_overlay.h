#ifndef MENU_OVERLAY_H
#define MENU_OVERLAY_H

/* Companion overlays: the EVENT overlays (OPTION, BUNIT, EQUIP, JOBSTTS and the others) that BATTLE and WORLD
 * load beside the scene, their requests and loading, the thread entries that open their screens, and the scene's
 * menu setup. */

#include "psx/types.h"

/* scene lifecycle */
/* 0: normal, 1: companion/editor transition, 2: AT-list selection. */
extern u16 g_menu_overlay_state;
/* Nonzero while a screen transition holds the menus (4 during the unit list fade-out). */
extern u16 g_menu_transition_state;

/* Loads the default menu palettes and rebuilds the text section pointers. */
void menu_init_palette_and_text(s32 unused_0);
/* Binds the scene's working pointers and clears its menu-session flags. */
void menu_init_subsystem_pointers(void);
/* Loads ATTACK.OUT with its scenario conditions and graphics and starts the deployment screen (returns 0), or
 * starts the map-load thread when deployment is skipped (returns 1). */
s32 menu_init_attack_resources_and_threads(void);
/* Frees every window buffer pair, the tpage-7 VRAM grid and the menu arena, and resets the thread scheduler. */
void menu_reset_subsystems(void);
/* 2 while a menu is being built, 4 while a menu thread runs, 0 when idle; BATTLE's body first returns
 * g_menu_transition_state when it is set. */
s32 menu_get_thread_activity(void);

/* companion overlays */
/* Companion overlay requests: index into the overlay LBA, size and load offset
 * tables (g_menu_companion_executable_lbas). */
typedef enum companion_overlay {
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
} companion_overlay_e;

/* Companion data file requests (the second LBA and size tables). */
typedef enum companion_file {
    COMPANION_FILE_WIN001 = 2,    /* EVENT/WIN001.BIN */
    COMPANION_FILE_BONUS = 3,     /* EVENT/BONUS.BIN */
    COMPANION_FILE_SPELL_MES = 4, /* EVENT/SPELL.MES */
} companion_file_e;

/* g_menu_companion_overlay_state: the overlay entry the frame driver calls, cleared when the overlay
 * returns. */
typedef enum companion_overlay_state {
    COMPANION_OVERLAY_STATE_IDLE = 0,
    COMPANION_OVERLAY_STATE_STUB = 1,      /* an empty function */
    COMPANION_OVERLAY_STATE_BUNIT = 2,     /* bunit_entrypoint */
    COMPANION_OVERLAY_STATE_JOBSTTS = 3,   /* jobstts_entrypoint */
    COMPANION_OVERLAY_STATE_EQUIP = 4,     /* equip_entrypoint */
    COMPANION_OVERLAY_STATE_CARD_SAVE = 5, /* card_save_run_menu */
} companion_overlay_state_e;

extern s32 g_menu_companion_overlay_state;
extern s32 g_menu_companion_overlay_primitives;
extern s32 g_menu_companion_executable_request_index;
extern s32 g_menu_companion_executable_loading;
extern s32 g_menu_companion_executable_lbas[];
extern s32 g_menu_companion_executable_sizes[];
extern s32 g_menu_companion_executable_offsets[];
extern s32 g_menu_companion_data_file_request_index;
extern s32 g_menu_companion_data_file_loading;
extern s32 g_menu_companion_data_file_lbas[];
extern s32 g_menu_companion_data_file_sizes[];
extern u8* g_menu_companion_data_file_buffer;
extern char g_menu_wait_for_file_allocation_message[];

/* Publishes an executable request and waits for it to clear. */
void menu_request_open_companion_executable(s32 request_index);
/* The per-frame drivers of the executable and data file requests: they start the disc read on the main stack,
 * poll until it finishes and clear the request. */
void menu_open_companion_executable(void);
void menu_open_companion_data_file(void);
/* Requests SMALL.OUT without waiting; menu_poll_small_load drives the load during map initialization and returns
 * 1 while it is pending. */
void menu_request_open_small(void);
s32 menu_poll_small_load(void);
/* The overlay entry of COMPANION_OVERLAY_STATE_STUB. */
void menu_run_companion_overlay_stub(void);

/* screen starters */
/* Thread entries of the menu rows that open an overlay screen. */
void menu_open_bunit(void);
void menu_open_debugchr(void);
void menu_open_mounted_unit_status_screen(void);
void menu_open_option_at_list(void);
void menu_open_option_crystal_pickup_panel(void);
void menu_open_option_game_options(void);
void menu_run_equip_thread(void);
void menu_run_jobstts_thread(void);
void menu_start_mini_menu_display_thread(void);
/* Waits for the camera and the menu transition, then runs the Triangle mini menu. */
void menu_run_triangle_selection(void);

/* overlay list threads */
/* EQUIP, the menu library and WORLD each build their own bodies of these. */
void menu_run_ability_list_thread(void);
void menu_run_equipment_list_thread(void);
/* EQUIP and WORLD each build their own body. */
s32 menu_update_equipment(void);

#endif
