#ifndef WORLD_H
#define WORLD_H

/* WORLD.BIN, the organization program (formation, shops, the memory card and the menu screens): world map locations,
 * screen requests, threads and the declarations its topic headers share. */

#include "fft/unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* locations */
/* World-map location ids (EVENT_SCRIPT_VAR_LOCATION), named as the location
 * name text (TEXT_ID_LOCATION_NAME_BASE + id) spells them. The first fifteen
 * are the towns; a town's id is also its outfitter's shop id and its bit,
 * counted from the top, in g_world_shop_item_availability. */
typedef enum world_location_id {
    WORLD_LOCATION_LESALIA = 0,
    WORLD_LOCATION_RIOVANES = 1,
    WORLD_LOCATION_IGROS = 2,
    WORLD_LOCATION_LIONEL = 3,
    WORLD_LOCATION_LIMBERRY = 4,
    WORLD_LOCATION_ZELTENNIA = 5,
    WORLD_LOCATION_GARILAND = 6,
    WORLD_LOCATION_YARDOW = 7,
    WORLD_LOCATION_GOLAND = 8,
    WORLD_LOCATION_DORTER = 9,
    WORLD_LOCATION_ZALAND = 10,
    WORLD_LOCATION_GOUG = 11,
    WORLD_LOCATION_WARJILIS = 12,
    WORLD_LOCATION_BERVENIA = 13,
    WORLD_LOCATION_ZARGHIDAS = 14,
    WORLD_LOCATION_FORT_ZEAKDEN = 15,
    WORLD_LOCATION_MUROND_HOLY_PLACE = 16,
    WORLD_LOCATION_THIEVES_FORT = 17,
    WORLD_LOCATION_ORBONNE_MONASTERY = 18,
    WORLD_LOCATION_GOLGORAND_EXECUTION_SITE = 19,
    WORLD_LOCATION_MUROND_DEATH_CITY = 20,
    WORLD_LOCATION_BETHLA_GARRISON = 21,
    WORLD_LOCATION_DEEP_DUNGEON = 22,
    WORLD_LOCATION_NELVESKA_TEMPLE = 23,
    WORLD_LOCATION_MANDALIA_PLAINS = 24,
    WORLD_LOCATION_FOVOHAM_PLAINS = 25,
    WORLD_LOCATION_SWEEGY_WOODS = 26,
    WORLD_LOCATION_BERVENIA_VOLCANO = 27,
    WORLD_LOCATION_ZEKLAUS_DESERT = 28,
    WORLD_LOCATION_LENALIA_PLATEAU = 29,
    WORLD_LOCATION_ZIGOLIS_SWAMP = 30,
    WORLD_LOCATION_YUGUO_WOODS = 31,
    WORLD_LOCATION_ARAGUAY_WOODS = 32,
    WORLD_LOCATION_GROG_HILL = 33,
    WORLD_LOCATION_BED_DESERT = 34,
    WORLD_LOCATION_ZIREKILE_FALLS = 35,
    WORLD_LOCATION_DOLBODAR_SWAMP = 36,
    WORLD_LOCATION_BARIAUS_HILL = 37,
    WORLD_LOCATION_DOGUOLA_PASS = 38,
    WORLD_LOCATION_BARIAUS_VALLEY = 39,
    WORLD_LOCATION_FINATH_RIVER = 40,
    WORLD_LOCATION_POESKAS_LAKE = 41,
    WORLD_LOCATION_GERMINAS_PEAK = 42,
} world_location_id_e;

/* Counts and the random-battle location range. */
enum {
    WORLD_LOCATION_TOWN_COUNT = 15,
    WORLD_LOCATION_FIELD_FIRST = WORLD_LOCATION_MANDALIA_PLAINS,
    WORLD_LOCATION_FIELD_COUNT = 19,
    WORLD_LOCATION_COUNT = 43,
};

extern u8 g_world_location_names_resident[];

/* screen requests */
/* State of a screen request raised from the world map menu (the
 * g_world_*_request_state globals stepped by world_menu_run_frame):
 * fade out, run the screen, fade back in. States 4 to 7 only count frames. */
typedef enum world_screen_request_state {
    WORLD_SCREEN_REQUEST_IDLE = 0,
    WORLD_SCREEN_REQUEST_START_FADE_OUT = 1,
    WORLD_SCREEN_REQUEST_WAIT_FADE_OUT = 2,
    WORLD_SCREEN_REQUEST_RUN = 3,
    WORLD_SCREEN_REQUEST_START_FADE_IN = 8,
    WORLD_SCREEN_REQUEST_WAIT_FADE_IN = 9,
} world_screen_request_state_e;

/* world map */
extern u16 g_world_map_append_state;
extern u16 g_world_map_reload_state;
extern s16 g_world_map_brave_story_request_state; /* world_screen_request_state_e */
s32 world_screen_is_busy(void);
void world_map_confirm_destination(void);

/* system */
void world_game_reset(void);

/* threads */
/* Slots that the WORLD screens use for threads of their own; thread.h has the
 * maps shared with BATTLE, wldcore.h and open.h the WLDCORE and OPENING ones. */
typedef enum world_thread_slot {
    WORLD_THREAD_SLOT_MENU_GROUP = 2,           /* world_menu_thread_group_supervisor_thread */
    WORLD_THREAD_SLOT_EQUIP_ABILITY_PANEL = 10, /* the equipment (change banner) or the ability panel */
    WORLD_THREAD_SLOT_PREVIEW_STATS = 11,       /* the numeric panel thread of the preview stats window */
    WORLD_THREAD_SLOT_UPPER_MENU_CHILD = 14,    /* the child of the menu in slot 15, or a wait-forever thread
                                                 * standing in for it */
    WORLD_THREAD_SLOT_UPPER_MENU = 15,          /* a second menu beside the one in slot 6 */
    WORLD_THREAD_SLOT_TUTORIAL_TEXT = 16,       /* tutorial script text: the slot past the sixteen reset ones */
} world_thread_slot_e;

/* menu_run_at_list runs its window frame thread in the upper menu's slot. */
enum {
    WORLD_AT_LIST_THREAD_SLOT_WINDOW_FRAME = 15,
};

s32 world_thread_is_task_active(s32 unused_0);
void world_thread_yield_forever(void);
void world_thread_request_redraw(s32 thread_id);
void world_thread_request_stop(s32 thread_id);

/* gfx */
/* Texture-page 7 uses a grid of 16-pixel square allocation cells. */
enum {
    WORLD_TPAGE7_GRID_ROWS = 15,
    WORLD_TPAGE7_GRID_COLUMNS = 16,
    WORLD_TPAGE7_CELL_PIXELS = 16,
    WORLD_TPAGE7_CELL_PIXEL_SHIFT = 4,
    WORLD_TPAGE7_CELL_PIXEL_MASK = 0x0f,
    WORLD_TPAGE7_GRID_OCCUPIED = 0xff,
};

extern u16 g_world_portrait_clut_by_sheet_id[]; /* 16-bit CLUT value per sprite-sheet id */
extern u16 g_world_help_portrait_clut_by_sheet_id[];

/* unit */
void resolve_unit_status_changes(void);
s32 unit_has_adjacent_monster_skill_support(struct unit_stats* unit);

#endif
