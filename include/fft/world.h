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
 * counted from the top, in g_shop_item_availability. */
typedef enum location_id {
    LOCATION_LESALIA = 0,
    LOCATION_RIOVANES = 1,
    LOCATION_IGROS = 2,
    LOCATION_LIONEL = 3,
    LOCATION_LIMBERRY = 4,
    LOCATION_ZELTENNIA = 5,
    LOCATION_GARILAND = 6,
    LOCATION_YARDOW = 7,
    LOCATION_GOLAND = 8,
    LOCATION_DORTER = 9,
    LOCATION_ZALAND = 10,
    LOCATION_GOUG = 11,
    LOCATION_WARJILIS = 12,
    LOCATION_BERVENIA = 13,
    LOCATION_ZARGHIDAS = 14,
    LOCATION_FORT_ZEAKDEN = 15,
    LOCATION_MUROND_HOLY_PLACE = 16,
    LOCATION_THIEVES_FORT = 17,
    LOCATION_ORBONNE_MONASTERY = 18,
    LOCATION_GOLGORAND_EXECUTION_SITE = 19,
    LOCATION_MUROND_DEATH_CITY = 20,
    LOCATION_BETHLA_GARRISON = 21,
    LOCATION_DEEP_DUNGEON = 22,
    LOCATION_NELVESKA_TEMPLE = 23,
    LOCATION_MANDALIA_PLAINS = 24,
    LOCATION_FOVOHAM_PLAINS = 25,
    LOCATION_SWEEGY_WOODS = 26,
    LOCATION_BERVENIA_VOLCANO = 27,
    LOCATION_ZEKLAUS_DESERT = 28,
    LOCATION_LENALIA_PLATEAU = 29,
    LOCATION_ZIGOLIS_SWAMP = 30,
    LOCATION_YUGUO_WOODS = 31,
    LOCATION_ARAGUAY_WOODS = 32,
    LOCATION_GROG_HILL = 33,
    LOCATION_BED_DESERT = 34,
    LOCATION_ZIREKILE_FALLS = 35,
    LOCATION_DOLBODAR_SWAMP = 36,
    LOCATION_BARIAUS_HILL = 37,
    LOCATION_DOGUOLA_PASS = 38,
    LOCATION_BARIAUS_VALLEY = 39,
    LOCATION_FINATH_RIVER = 40,
    LOCATION_POESKAS_LAKE = 41,
    LOCATION_GERMINAS_PEAK = 42,
} location_id_e;

/* Counts and the random-battle location range. */
enum {
    LOCATION_TOWN_COUNT = 15,
    LOCATION_FIELD_FIRST = LOCATION_MANDALIA_PLAINS,
    LOCATION_FIELD_COUNT = 19,
    LOCATION_COUNT = 43,
};

extern u8 g_world_map_location_names[];

/* screen requests */
/* State of a screen request raised from the world map menu (the
 * g_world_*_request_state globals stepped by screen_menu_frame):
 * fade out, run the screen, fade back in. States 4 to 7 only count frames. */
typedef enum world_map_request_state {
    WORLD_MAP_REQUEST_IDLE = 0,
    WORLD_MAP_REQUEST_START_FADE_OUT = 1,
    WORLD_MAP_REQUEST_WAIT_FADE_OUT = 2,
    WORLD_MAP_REQUEST_RUN = 3,
    WORLD_MAP_REQUEST_START_FADE_IN = 8,
    WORLD_MAP_REQUEST_WAIT_FADE_IN = 9,
} world_map_request_state_e;

/* world map */
extern u16 g_world_map_append_state;
extern u16 g_world_map_reload_state;
extern s16 g_world_map_brave_story_request_state; /* world_map_request_state_e */
s32 screen_is_busy(void);
void world_map_confirm_destination(void);

/* system */
void screen_reset_game(void);

/* threads */
/* Slots that the WORLD screens use for threads of their own; thread.h has the
 * maps shared with BATTLE, wldcore.h and open.h the WLDCORE and OPENING ones. */
typedef enum screen_thread_slot {
    SCREEN_THREAD_SLOT_MENU_GROUP = 2,           /* world_map_menu_group_thread */
    SCREEN_THREAD_SLOT_EQUIP_ABILITY_PANEL = 10, /* the equipment (change banner) or the ability panel */
    SCREEN_THREAD_SLOT_PREVIEW_STATS = 11,       /* the numeric panel thread of the preview stats window */
    SCREEN_THREAD_SLOT_UPPER_MENU_CHILD = 14,    /* the child of the menu in slot 15, or a wait-forever thread
                                                  * standing in for it */
    SCREEN_THREAD_SLOT_UPPER_MENU = 15,          /* a second menu beside the one in slot 6 */
    SCREEN_THREAD_SLOT_TUTORIAL_TEXT = 16,       /* tutorial script text: the slot past the sixteen reset ones */
} screen_thread_slot_e;

/* at_list_run runs its window frame thread in the upper menu's slot. */
enum {
    SCREEN_THREAD_SLOT_AT_LIST_FRAME = 15,
};

s32 screen_thread_is_task_active(s32 unused_0);
void screen_thread_yield_forever(void);
void screen_thread_request_redraw(s32 thread_id);
void screen_thread_request_stop(s32 thread_id);

/* gfx */
/* Texture-page 7 uses a grid of 16-pixel square allocation cells. */
enum {
    SCREEN_TPAGE7_GRID_ROWS = 15,
    SCREEN_TPAGE7_GRID_COLUMNS = 16,
    SCREEN_TPAGE7_CELL_PIXELS = 16,
    SCREEN_TPAGE7_CELL_PIXEL_SHIFT = 4,
    SCREEN_TPAGE7_CELL_PIXEL_MASK = 0x0f,
    SCREEN_TPAGE7_GRID_OCCUPIED = 0xff,
};

extern u16 g_screen_portrait_clut_by_sheet_id[]; /* 16-bit CLUT value per sprite-sheet id */
extern u16 g_screen_help_portrait_clut_by_sheet_id[];

/* unit */
void resolve_unit_status_changes(void);
s32 unit_has_adjacent_monster_skill_support(struct unit_stats* unit);

#endif
