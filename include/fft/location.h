#ifndef LOCATION_H
#define LOCATION_H

/* A town, castle or battlefield on the World Map: its menu (Bar, Shop, Fur Shop, Soldier Office), name label and
 * picture, route travel between locations, random and Deep Dungeon battles, and location_script_, the world map's own
 * script machine that decides what happens at a location (events, choices, paths), with its opcode handlers. */

#include "fft/menu_stack.h"
#include "psx/libgs.h"
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
    LOCATION_GERMINAS_PEAK = 42
} location_id_e;

/* Counts and the random-battle location range. */
enum {
    LOCATION_TOWN_COUNT = 15,
    LOCATION_FIELD_FIRST = LOCATION_MANDALIA_PLAINS,
    LOCATION_FIELD_COUNT = 19,
    LOCATION_COUNT = 43
};

/* Provisional: four-byte per-location records reached through g_location_records. */
typedef struct location_record {
    u8 _unused_00[2];
    u8 picture; /* 0x02; location picture set + 1, 0 for none */
    u8 is_town; /* 0x03; 1 offers the town entries TEXT_ID_BAR to _SOLDIER_OFFICE in the location menu */
} location_record_t;

extern location_record_t* g_location_records;
extern s32 g_location_label_shade;

/* The location whose name label is drawn over its map dot. */
extern s32 g_location_selected;

s32 location_load_picture(s32 index);
void location_draw_label(GsOT* ot);

/* location menu */
/* The LOCATION level's cursor and picture records, fade, and choice list. */
typedef struct location_menu_level {
    menu_stack_slot_set_t slots; /* 0x00; slots.result is 1 on confirm, 0 on cancel */
    s32 delay;                   /* 0x14; 8 at push, counted down before input */
    s32 fade;                    /* 0x18; starts at 8, increases to 128, then clears */
    u8 _unused_1c[0x14];
    s32 entry_count; /* 0x30 */
    s32 entries[10]; /* 0x34; choice text ids populated before the level opens */
} location_menu_level_t;
typedef char location_menu_level_size_must_be_92[sizeof(location_menu_level_t) == 92 ? 1 : -1];

s32 location_build_menu_entries(s32 location_id, s32* out);
void location_push_menu_level(s32 menu);
void location_suspend_menu_level(menu_stack_record_t* level);
void location_resume_menu_level(menu_stack_record_t* level);

/* travel */
/* Provisional: the level a route travel runs under. travel_step uses the leading window index,
 * the location id, the pending result, and the choice text ids it collects when the travel ends. */
typedef struct travel_level {
    s32 window_index; /* 0x00 */
    u8 _unused_04[8];
    s32 location_id; /* 0x0c */
    u8 _unused_10[0x20];
    s32 pending_result;      /* 0x30 */
    s32 choice_text_ids[10]; /* 0x34; retained as location_menu.entries after arrival */
} travel_level_t;
typedef char travel_level_size_must_be_92[sizeof(travel_level_t) == 92 ? 1 : -1];

enum { TRAVEL_PATH_CAPACITY = 16 };

/* A path of route legs, as the route search finds it. */
typedef struct travel_path {
    s32 routes[TRAVEL_PATH_CAPACITY];
} travel_path_t;
typedef char travel_path_size_must_be_64[sizeof(travel_path_t) == 64 ? 1 : -1];

/* A path entry packs the route id in its low byte and the direction in bit 8. */
enum { TRAVEL_PATH_ROUTE_MASK = 0xff, TRAVEL_PATH_REVERSED = 0x100 };

/* Values of travel_t.flags. */
typedef enum travel_flag {
    TRAVEL_ACTIVE = 0x01,  /* a path was found and is being traveled */
    TRAVEL_REVERSED = 0x02 /* the current leg runs its route end to start */
} travel_flag_e;

/* Provisional: the route travel state (g_travel), filled by the route search
 * travel_find_best_path, staged per route leg by travel_begin_segment and stepped by
 * travel_step. The vertices are SVECTORs: their 2-byte alignment is what makes
 * `previous_position = position` take GCC's unaligned block-move path (lwl/lwr, swl/swr) that the target emits. */
typedef struct travel {
    s32 flags;                 /* 0x00; travel_flag_e */
    s32 route_id;              /* 0x04; route & 0xff, index into g_world_map_routes */
    s32 segment_index;         /* 0x08; current segment of the leg, the step for travel_begin_segment */
    s32 segment_count;         /* 0x0c; travel_begin_segment result (record count - 1) */
    s32 segment_frame;         /* 0x10; frames into the segment, interpolation numerator */
    s32 segment_frames;        /* 0x14; point distance * 2, interpolation denominator */
    travel_path_t route_path;  /* 0x18; best path found */
    s32 route_index;           /* 0x58; route_path.routes index of the current leg */
    s32 best_path_length;      /* 0x5c */
    s32 best_cost;             /* 0x60; reset to 99999 */
    s32 best_length_bound;     /* 0x64; reset to 99999 */
    s32 leg_start_location;    /* 0x68; route record start (end when reversed) */
    s32 leg_end_location;      /* 0x6c; becomes the current location and script variable 0x31 on arrival */
    s32 heading;               /* 0x70; route point heading (+0x800 when reversed), copied to the marker angle */
    s32 menu_level;            /* 0x74; parent menu depth */
    SVECTOR previous_position; /* 0x78 */
    SVECTOR position;          /* 0x80 */
    SVECTOR target_position;   /* 0x88 */
    s32 arrival_location;      /* 0x90; arrival location id + 1 */
} travel_t;
typedef char travel_size_must_be_148[sizeof(travel_t) == 148 ? 1 : -1];

extern travel_t g_travel;

/* Map published to the event script when the world-map loop exits. */
extern s32 g_travel_next_map_id;

s32 travel_start(s32 from_location, s32 to_location);
void travel_step(void);

/* random battles */
/* One random-encounter tier: the story-progress threshold that selects it,
 * the chance an encounter happens (a roll of 0..99 at or below it), and the
 * high-bit-first mask of which of the entry's eight ENTDs may be drawn. */
typedef struct random_battle_tier {
    u8 progress_threshold; /* 0x00 */
    u8 encounter_chance;   /* 0x01 */
    u8 encounter_mask;     /* 0x02 */
} random_battle_tier_t;

/* One road into a world-map location. Three per location, 19 locations
 * (ids 24..42), reached through the pointer cell g_random_battle_roads. */
typedef struct random_battle_road {
    u8 route;                      /* 0x00 */
    u8 deployment_squad_id;        /* 0x01: deployment zone id = this + 0x200 */
    random_battle_tier_t tiers[4]; /* 0x02 */
    u8 entd_ids[8];                /* 0x0e */
    u8 battle_map_id;              /* 0x16 */
    u8 script_variable_id;         /* 0x17 */
} random_battle_road_t;

typedef struct deep_dungeon_battle {
    u8 map_id;   /* 0x00 */
    u8 squad_id; /* 0x01: deployment zone id */
    u8 entd_ids[8];
} deep_dungeon_battle_t;

extern s32 g_random_battle_entd_id;
extern s32 g_random_battle_squad_id;

/* location script */
/* location_script_state_t.flags. The interpreter clears them before each block. Comparisons set CONDITION_FAILED
 * when their predicate rejects the block; action opcodes set ACTION_REACHED to end the block and an action bit when
 * they stage a request. */
typedef enum location_script_result_flags {
    LOCATION_SCRIPT_RESULT_ACTION_REACHED = 0x001,
    LOCATION_SCRIPT_RESULT_CONDITION_FAILED = 0x002
} location_script_result_flags_e;

typedef enum location_script_action_flags {
    LOCATION_SCRIPT_ACTION_CHOICE = 0x004,
    LOCATION_SCRIPT_ACTION_LOAD_EVENT = 0x008,
    LOCATION_SCRIPT_ACTION_TEXT = 0x010,
    LOCATION_SCRIPT_ACTION_DEEP_DUNGEON_ENTRY = 0x020,
    LOCATION_SCRIPT_ACTION_SET_VAR_2 = 0x040,
    LOCATION_SCRIPT_ACTION_DRAW_PATH = 0x080,
    LOCATION_SCRIPT_ACTION_ERASE_PATH = 0x100,
    LOCATION_SCRIPT_ACTION_FOCUS_LOCATION = 0x200,
    LOCATION_SCRIPT_ACTION_DRAW_LOCATION = 0x400,
    LOCATION_SCRIPT_ACTION_ERASE_LOCATION = 0x800,
    LOCATION_SCRIPT_ACTION_PATH_MASK = LOCATION_SCRIPT_ACTION_DRAW_PATH | LOCATION_SCRIPT_ACTION_ERASE_PATH,
    LOCATION_SCRIPT_ACTION_LOCATION_VISIBILITY_MASK
    = LOCATION_SCRIPT_ACTION_DRAW_LOCATION | LOCATION_SCRIPT_ACTION_ERASE_LOCATION,
    LOCATION_SCRIPT_ACTION_MAP_MENU_MASK = LOCATION_SCRIPT_ACTION_PATH_MASK | LOCATION_SCRIPT_ACTION_FOCUS_LOCATION
        | LOCATION_SCRIPT_ACTION_LOCATION_VISIBILITY_MASK
} location_script_action_flags_e;

/* What an action opcode stores: ACTION_REACHED with its action bit. */
typedef enum location_script_request_flags {
    LOCATION_SCRIPT_REQUEST_CHOICE = LOCATION_SCRIPT_RESULT_ACTION_REACHED | LOCATION_SCRIPT_ACTION_CHOICE,
    LOCATION_SCRIPT_REQUEST_LOAD_EVENT = LOCATION_SCRIPT_RESULT_ACTION_REACHED | LOCATION_SCRIPT_ACTION_LOAD_EVENT,
    LOCATION_SCRIPT_REQUEST_TEXT = LOCATION_SCRIPT_RESULT_ACTION_REACHED | LOCATION_SCRIPT_ACTION_TEXT,
    LOCATION_SCRIPT_REQUEST_DEEP_DUNGEON_ENTRY
    = LOCATION_SCRIPT_RESULT_ACTION_REACHED | LOCATION_SCRIPT_ACTION_DEEP_DUNGEON_ENTRY,
    LOCATION_SCRIPT_REQUEST_SET_VAR_2 = LOCATION_SCRIPT_RESULT_ACTION_REACHED | LOCATION_SCRIPT_ACTION_SET_VAR_2,
    LOCATION_SCRIPT_REQUEST_DRAW_PATH = LOCATION_SCRIPT_RESULT_ACTION_REACHED | LOCATION_SCRIPT_ACTION_DRAW_PATH,
    LOCATION_SCRIPT_REQUEST_ERASE_PATH = LOCATION_SCRIPT_RESULT_ACTION_REACHED | LOCATION_SCRIPT_ACTION_ERASE_PATH,
    LOCATION_SCRIPT_REQUEST_FOCUS_LOCATION
    = LOCATION_SCRIPT_RESULT_ACTION_REACHED | LOCATION_SCRIPT_ACTION_FOCUS_LOCATION,
    LOCATION_SCRIPT_REQUEST_DRAW_LOCATION
    = LOCATION_SCRIPT_RESULT_ACTION_REACHED | LOCATION_SCRIPT_ACTION_DRAW_LOCATION,
    LOCATION_SCRIPT_REQUEST_ERASE_LOCATION
    = LOCATION_SCRIPT_RESULT_ACTION_REACHED | LOCATION_SCRIPT_ACTION_ERASE_LOCATION
} location_script_request_flags_e;

/* The interpreter's state; the opcode handlers read ip and data here. It is one object through the choice and
 * argument values: location_script_opcode_1a_request_choice reaches ip at -4 and the arguments at +0x10 from its
 * &choice_vars base. */
typedef struct location_script_state {
    s32 flags;          /* 0x00; location_script_result_flags_e | location_script_action_flags_e */
    u16* list;          /* 0x04; script list for the current location */
    u16* data;          /* 0x08 */
    u16 ip;             /* 0x0c */
    s32 choice_vars[4]; /* 0x10; staged Choice variables */
    /* 0x20: script request arguments; the staged Choice values share them. */
    s32 args[4];
} location_script_state_t;
typedef char location_script_state_size_must_be_48[sizeof(location_script_state_t) == 48 ? 1 : -1];

/* The loaded world map script: a 16-bit offset per location to its list of conditional blocks. */
extern u16* g_location_script_base;
extern location_script_state_t g_location_script_state;

s32 location_script_run(s32 location, s32 action_mask);
void location_script_dispatch_requests(void);
void location_script_opcode_19_request_load_event(void);
void location_script_opcode_1d_request_text(void);
void location_script_opcode_1e_request_deep_dungeon_entry(void);
void location_script_opcode_1f_request_set_var_2(void);
void location_script_opcode_20_request_draw_path(void);
void location_script_opcode_21_request_erase_path(void);

#endif
