/* WORLD/WORLD.BIN: world map, menus and event scripts. */
#ifndef WORLD_H
#define WORLD_H

#include "fft/data.h"
#include "fft/gfx.h"
#include "fft/job.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* dead unit */

extern s16 g_world_dead_unit_menu_result; /* primary menu result */
extern s16 g_world_dead_unit_move_result; /* secondary (move) menu result */

/* help */

extern u16 g_world_help_portrait_clut_by_sheet_id[];

/* map */
/* State of a screen request raised from the world map menu (the
 * g_world_*_request_state globals stepped by world_formation_run_menu_frame):
 * fade out, run the screen, fade back in. States 4 to 7 only count frames. */
typedef enum world_screen_request_state {
    WORLD_SCREEN_REQUEST_IDLE = 0,
    WORLD_SCREEN_REQUEST_START_FADE_OUT = 1,
    WORLD_SCREEN_REQUEST_WAIT_FADE_OUT = 2,
    WORLD_SCREEN_REQUEST_RUN = 3,
    WORLD_SCREEN_REQUEST_START_FADE_IN = 8,
    WORLD_SCREEN_REQUEST_WAIT_FADE_IN = 9,
} world_screen_request_state_e;

extern u16 g_world_map_append_state;
extern u16 g_world_map_reload_state;
extern s16 g_world_map_root_request_state; /* world_screen_request_state_e */
s32 world_map_is_busy(void);

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

void world_map_confirm_destination(void);

/* name */
/* Editing keys of world_name_apply_entry_key; any other key is the character
 * written at the cursor. */
typedef enum world_name_entry_key {
    WORLD_NAME_ENTRY_KEY_BACKSPACE = -1,
    WORLD_NAME_ENTRY_KEY_DELETE = -2,
    WORLD_NAME_ENTRY_KEY_INSERT_SPACE = -3,
    WORLD_NAME_ENTRY_KEY_CURSOR_LEFT = -4,
    WORLD_NAME_ENTRY_KEY_CURSOR_RIGHT = -5,
} world_name_entry_key_e;

/* script */
/* Operand pairs that one WORLD script_execute_event_variant opcode stores together;
 * each second 16-bit value sits off a 32-bit alignment boundary in WORLD bss, so each pair is
 * one object. */
typedef struct world_event_command_args {
    s16 arg1;
    s16 arg2;
} world_event_command_args_t;

typedef struct world_event_music_switch {
    s16 volume;
    s16 fade;
} world_event_music_switch_t;

/* status */

/* Per-byte status removal masks applied while an event stages a unit. */

/* system */

extern s32 g_menu_companion_executable_lbas[];    /* 0x80156388, indexed by request */
extern s32 g_menu_companion_executable_sizes[];   /* 0x801563c8 */
extern s32 g_menu_companion_executable_offsets[]; /* 0x80156408, relative to the overlay load address */
extern s32 g_menu_companion_executable_loading;   /* 0x801564cc */
void world_game_reset(void);

/* thread */

/* Slots that the WORLD screens use for threads of their own; thread.h has the
 * maps shared with BATTLE, wldcore.h and open.h the WLDCORE and OPENING ones. */
typedef enum world_thread_slot {
    WORLD_THREAD_SLOT_MENU_GROUP = 2,           /* world_menu_thread_group_supervisor_thread */
    WORLD_THREAD_SLOT_EQUIP_ABILITY_PANEL = 10, /* the equipment (change banner) or the ability panel */
    WORLD_THREAD_SLOT_PREVIEW_STATS = 11,       /* the numeric editor thread of the preview stats window */
    WORLD_THREAD_SLOT_UPPER_MENU_CHILD = 14,    /* the child of the menu in slot 15, or a wait-forever thread
                                                 * standing in for it */
    WORLD_THREAD_SLOT_UPPER_MENU = 15,          /* a second menu beside the one in slot 6 */
    WORLD_THREAD_SLOT_TUTORIAL_TEXT = 16,       /* tutorial script text: the slot past the sixteen reset ones */
} world_thread_slot_e;

/* menu_run_at_list_variant runs its window frame thread in the upper menu's slot. */
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

/* Separate decreasing/increasing RGB-intensity flags.
 * The getter packs them into bits 0/1 and returns 0 when disabled. */
typedef enum world_fade_state {
    WORLD_FADE_STATE_NONE = 0,
    WORLD_FADE_STATE_DECREASING = 1,
    WORLD_FADE_STATE_INCREASING = 2
} world_fade_state_e;

/* 32-bit flags that lead every display object: the WLDCORE window and render
 * records and the OPEN render records. The two kind bits select the draw
 * routine; an object with either BLANK or HIDDEN set is skipped. */
typedef enum world_render_record_flag {
    WORLD_RENDER_RECORD_ANIMATED = 0x001,  /* 36-byte record drawn from its animation sequence */
    WORLD_RENDER_RECORD_IMAGE = 0x002,     /* 52- or 56-byte record drawn as one sprite */
    WORLD_RENDER_RECORD_8BIT_CLUT = 0x004, /* image: 8-bit texture instead of 4-bit */
    WORLD_RENDER_RECORD_BLANK = 0x008,     /* nothing to draw, whatever HIDDEN says */
    WORLD_RENDER_RECORD_HIDDEN = 0x010,    /* the show/hide toggle */
    /* Semi-transparent at the rate in BLEND_RATE_MASK; a WLDCORE animated object otherwise takes
     * both from each cel part. */
    WORLD_RENDER_RECORD_SEMI_TRANSPARENT = 0x020,
    WORLD_RENDER_RECORD_BLEND_RATE_MASK = 0x0c0,
    WORLD_RENDER_RECORD_POP_IN = 0x100,   /* image: the opening animation is running; clears itself */
    WORLD_RENDER_RECORD_UNTINTED = 0x400, /* WLDCORE: the color is used as is, without the fade tint */
    /* WLDCORE animated object: steps its frame counter even while system flag 0x08000000 is set. */
    WORLD_RENDER_RECORD_ANIMATE_WHILE_PAUSED = 0x800,
} world_render_record_flag_e;

/* One frame of a WLDCORE display-object or OPEN sprite-actor animation
 * sequence: the cel (part list) it draws and how long it holds. duration
 * 0xffff pins the frame. */
typedef struct world_sprite_animation_frame {
    u16 cel;      /* 0x00 */
    u16 duration; /* 0x02 */
} world_sprite_animation_frame_t;

/* An animation: a frame count followed by its frames. */
typedef struct world_sprite_animation_sequence {
    s32 frame_count;                          /* 0x00 */
    world_sprite_animation_frame_t frames[1]; /* 0x04 */
} world_sprite_animation_sequence_t;

/* Serialized cel header, followed by part_count eight-byte sprite parts.
 * Origins pack the x coordinate in the low half and y in the high half. */
typedef struct world_sprite_cel_header {
    u32 part_count;
    u32 packed_texture_page_origin;
    u32 packed_clut_origin;
} world_sprite_cel_header_t;
typedef char world_sprite_cel_header_size_must_be_12[sizeof(world_sprite_cel_header_t) == 12 ? 1 : -1];

/* One serialized cel part. Offsets are biased by 0x80; texture_flags
 * selects vertical/horizontal UV flips and eight-bit texture depth. */
typedef struct world_sprite_part {
    u32 x_offset : 8;
    u32 y_offset : 8;
    u32 palette_index : 8;
    u32 texture_flags : 3;
    u32 use_part_palette : 1;
    u32 blend_rate : 2;
    u32 semi_transparent : 1;
    u32 hidden : 1;
    u8 height;
    u8 width;
    u8 v;
    u8 u;
} world_sprite_part_t;
typedef char world_sprite_part_size_must_be_8[sizeof(world_sprite_part_t) == 8 ? 1 : -1];

/* Animated display objects used by WLDCORE and OPEN. sequence selects a row
 * of the module's animation table; a zero palette keeps each sprite part's CLUT. */
typedef struct world_animated_render_record {
    s32 flags;          /* 0x00; world_render_record_flag_e */
    s32 sequence;       /* 0x04 */
    s32 priority;       /* 0x08; ordering-table priority */
    s32 frame_index;    /* 0x0c; current sequence frame */
    s32 anim_counter;   /* 0x10; frames spent on the current frame */
    s32 palette;        /* 0x14 */
    point32_t position; /* 0x18; screen coordinates */
    CVECTOR color;      /* 0x20; RGB tint */
} world_animated_render_record_t;
typedef char world_animated_render_record_size_must_be_36[sizeof(world_animated_render_record_t) == 36 ? 1 : -1];

/* Provisional: the column view of the 36-byte WLDCORE window records and OPEN
 * render records, rebased on their x/y pair (+0x18) and keeping the 0x24-byte
 * stride. */
typedef struct wldcore_window_xy {
    s32 x;
    s32 y;
    u8 _unused_08[0x1c];
} wldcore_window_xy_t;
typedef char wldcore_window_xy_size_must_be_36[sizeof(wldcore_window_xy_t) == 36 ? 1 : -1];

extern u16 g_world_portrait_clut_by_sheet_id[]; /* 16-bit CLUT value per sprite-sheet id */

extern s32 g_active_graphics_buffer_index;

void text_set_blit_destination(s16 x, s16 y);

/* job */

/* Result of world_formation_process_job_change, kept in
 * g_world_job_change_anim_state: the phase of the job-change dissolve. */
typedef enum world_job_change_anim {
    WORLD_JOB_CHANGE_ANIM_DONE = 0,
    WORLD_JOB_CHANGE_ANIM_DISSOLVE = 1,
    WORLD_JOB_CHANGE_ANIM_CLOSING = 2, /* the last 20 frames */
} world_job_change_anim_e;

/* An entry of the job list world_job_build_unit_list writes. */
enum {
    WORLD_JOB_LIST_ENTRY_ID_MASK = 0x3ff,
    /* Set in mode INCLUDE_OTHER_UNIT_UNLOCKS on the unit's current job and on
     * a job only another unit has unlocked: the job wheel shows the entry
     * but does not change to it. */
    WORLD_JOB_LIST_ENTRY_UNAVAILABLE = 0x4000,
};

/* The one mode world_job_build_unit_list tests; its callers also pass 0 and 1. */
enum { WORLD_JOB_LIST_MODE_INCLUDE_OTHER_UNIT_UNLOCKS = 2 };

/* tutorial */

/* Serialized tutorial commands; opcode selects the operand record. */
typedef struct world_tutorial_message_instruction {
    u8 opcode;
    u8 text_id_le[2];
    u8 y_offset;
} world_tutorial_message_instruction_t;
typedef char
    world_tutorial_message_instruction_size_must_be_4[sizeof(world_tutorial_message_instruction_t) == 4 ? 1 : -1];

typedef struct world_tutorial_dialog_instruction {
    u8 opcode;
    u8 text_id_le[2];
} world_tutorial_dialog_instruction_t;
typedef char
    world_tutorial_dialog_instruction_size_must_be_3[sizeof(world_tutorial_dialog_instruction_t) == 3 ? 1 : -1];

typedef struct world_tutorial_circle_instruction {
    u8 opcode;
    u8 x;
    u8 y;
    u8 radius;
    u8 scale_x;
    u8 scale_y;
} world_tutorial_circle_instruction_t;
typedef char
    world_tutorial_circle_instruction_size_must_be_6[sizeof(world_tutorial_circle_instruction_t) == 6 ? 1 : -1];

typedef struct world_tutorial_wait_instruction {
    u8 opcode;
    u8 frames;
} world_tutorial_wait_instruction_t;
typedef char world_tutorial_wait_instruction_size_must_be_2[sizeof(world_tutorial_wait_instruction_t) == 2 ? 1 : -1];

/* The current game is serialized here while a tutorial runs; its message
 * text and command stream follow the saved bytes at +0x1e00/+0x1e04. */
typedef struct world_tutorial_state {
    card_save_buffer_t saved_game;
    u8* messages;
    u8* script;
} world_tutorial_state_t;

typedef char world_tutorial_state_size_must_be_7688[sizeof(world_tutorial_state_t) == 7688 ? 1 : -1];

/*
 * WORLD.BIN tutorial command stream. Commands below 0x10 are one-byte shifts;
 * the rest are ID (1) followed by the listed parameters: 0x10 message id (2) +
 * y (1), 0x11 message id (2), 0x13 x, y, size, width mod, height mod (1 each),
 * 0x15 time (1).
 */
enum {
    WORLD_TUTORIAL_CMD_SHIFT_LIMIT = 0x10,
    WORLD_TUTORIAL_CMD_DISPLAY_MESSAGE = 0x10,
    WORLD_TUTORIAL_CMD_CHANGE_DIALOG = 0x11,
    WORLD_TUTORIAL_CMD_WAIT_FOR_MESSAGE = 0x12,
    WORLD_TUTORIAL_CMD_DRAW_CIRCLE = 0x13,
    WORLD_TUTORIAL_CMD_WAIT = 0x14,
    WORLD_TUTORIAL_CMD_WAIT_TIME = 0x15,
    WORLD_TUTORIAL_CMD_WAIT_FOR_BUTTON = 0x16,
    WORLD_TUTORIAL_CMD_END = 0x17,
};

/* formation */

enum {
    WORLD_FORMATION_PREVIEW_UNIT_INDEX = PARTY_ROSTER_SLOT_COUNT,
    WORLD_FORMATION_UNIT_RECORD_COUNT = WORLD_FORMATION_PREVIEW_UNIT_INDEX + 1,
};

/* Screen x and texture u of one background tile; two alternating rows of four. */
typedef struct world_formation_background_tile {
    u16 x;
    u16 u;
} world_formation_background_tile_t;
typedef char world_formation_background_tile_size_must_be_4[(sizeof(world_formation_background_tile_t) == 4) ? 1 : -1];

/*
 * The sprite-set -> script-variable table walked by the
 * dismissal tail, terminated by sprite_set == -1. The four-byte stride is
 * proved by the target advancing both its record pointer and its byte-offset
 * index by 4 per iteration.
 */
typedef struct world_formation_dismiss_entry {
    s16 sprite_set;  /* 0x00 */
    s16 variable_id; /* 0x02 */
} world_formation_dismiss_entry_t;

typedef enum world_monster_breed_rarity {
    WORLD_MONSTER_BREED_RARITY_NONE = 0,
    WORLD_MONSTER_BREED_RARITY_COMMON = 1,
    WORLD_MONSTER_BREED_RARITY_UNCOMMON = 2,
    WORLD_MONSTER_BREED_RARITY_RARE = 3,
} world_monster_breed_rarity_e;

enum { WORLD_MONSTER_BREED_RARITY_COUNT = 3 };

/* One inclusive date limit and its zodiac ID. The 13-entry WORLD table has
 * Capricorn at both calendar ends and does not contain Serpentarius. */
typedef struct world_zodiac_date_limit {
    calendar_date_t date;
    u8 zodiac;
} world_zodiac_date_limit_t;
typedef char world_zodiac_date_limit_size_must_be_3[(sizeof(world_zodiac_date_limit_t) == 3) ? 1 : -1];

enum {
    WORLD_ZODIAC_DATE_LIMIT_COUNT = 13,
    WORLD_MONTH_LENGTH_TABLE_COUNT = 13,
    WORLD_MONSTER_BREED_ENTRY_COUNT = JOB_ID_TIAMAT - JOB_ID_CHOCOBO + 1,
};

/* world_unit_t::proposition_status, copied from the party record:
 * the label the formation grid draws below a unit that is away. */
typedef enum world_formation_unit_proposition {
    WORLD_FORMATION_UNIT_PROPOSITION_LEAVING = 1,
    WORLD_FORMATION_UNIT_PROPOSITION_MISSING = 2,
} world_formation_unit_proposition_e;

/* Formation ability slots are selected by index in the equipment menu. */
typedef struct world_formation_ability_fields {
    u16 primary_skillset;   /* 0x00 */
    u16 secondary_skillset; /* 0x02 */
    s16 reaction_ability;   /* 0x04 */
    u16 support_ability;    /* 0x06 */
    s16 movement_ability;   /* 0x08 */
} world_formation_ability_fields_t;

typedef union world_formation_abilities {
    world_formation_ability_fields_t fields;
    u16 slots[5];
} world_formation_abilities_t;
typedef char world_formation_abilities_size_must_be_10[(sizeof(world_formation_abilities_t) == 10) ? 1 : -1];

/*
 * WORLD menu unit record, compacted after roster and menu-specific filtering.
 * Records at 0x801c8638 have stride 0x128; roster_slot preserves their roster
 * identity. The builder and sorter produce the
 * ordered g_world_unit_pointers list, not a roster-indexed array.
 * The byte fields from 0x70 on mirror the party_unit_t layout above. Magical
 * shield/accessory evade are provisionally placed at 0x50/0x52, after the
 * physical fields at 0x40/0x42, to follow the 16-bit sequence.
 */
typedef struct world_unit {
    menu_unit_status_record_t status;     /* 0x000; copied whole to the status banner */
    menu_unit_status_identity_t identity; /* 0x022 */
    s16 move;                             /* 0x030 */
    s16 speed;                            /* 0x032 */
    s16 jump;                             /* 0x034 */
    s16 right_weapon_power;               /* 0x036 */
    s16 left_weapon_power;                /* 0x038 */
    s16 right_weapon_evade;               /* 0x03a */
    s16 left_weapon_evade;                /* 0x03c */
    s16 uses_monster_skillset;            /* 0x03e; primary skillset is in the monster range 0xb0..0xdf */
    s16 two_handed;                       /* 0x040; unit_is_two_handing_weapon result */
    s16 physical_attack;                  /* 0x042 */
    s16 physical_class_evade;             /* 0x044 */
    s16 physical_shield_evade;            /* 0x046 */
    s16 physical_accessory_evade;         /* 0x048 */
    u8 _unused_04a[2];
    s16 magical_attack;                       /* 0x04c */
    s16 magical_class_evade;                  /* 0x04e */
    s16 magical_shield_evade;                 /* 0x050 */
    s16 magical_accessory_evade;              /* 0x052 */
    u16 equipment[MENU_EQUIPMENT_SLOT_COUNT]; /* 0x054; right hand, left hand, head, body, accessory */
    world_formation_abilities_t abilities;    /* 0x05e */
    u8 _unused_068[8];                        /* 0x068; spare tail of the stat-detail block */
    u8 gender_flags;                          /* 0x070; unit_flags_e (fft/unit.h) */
    u8 _unused_071;
    u8 sprite_set;               /* 0x072; "Sprite Set ID": party_unit_t.sprite_set / character identity */
    u8 equippable_item_types[4]; /* 0x073; bit per item type, read by can_unit_equip_item_id */
    u8 unlocked_jobs[UNIT_UNLOCKED_JOB_BYTE_COUNT];                                           /* 0x077 */
    u8 learned_abilities[UNIT_LEARNED_ABILITY_JOB_COUNT][UNIT_LEARNED_ABILITY_BYTES_PER_JOB]; /* 0x07a through 0x0b2 */
    u8 job_levels[UNIT_JOB_LEVEL_BYTE_COUNT];                                                 /* 0x0b3 */
    u8 _padding_0bd;                                                                          /* aligns job_points */
    unit_job_points_t job_points; /* 0x0be; current JP followed by total JP */
    u8 name[0x10];                /* 0x10e; "unit Nickname" */
    u8 monster_graphic;           /* 0x11e; added to formation graphic entry 0x3d when sprite_set == 0x82
                                     (world_formation_build_unit_graphic_entry) */
    u8 proposition_status;        /* 0x11f; "Proposition Byte": world_formation_unit_proposition_e, 0 when present */
    u16 birthday;                 /* 0x120; unit_stats_t birthday & 0x1ff; egg readers split
                                     it into nibbles */
    u16 egg_color; /* 0x122; party_unit_t 0xd2; the formation sprite drawer (0x8011751c) loads it as a 16-bit palette
                      index */
    u8 support_abilities[4]; /* 0x124; copy of unit_stats_t.ability_flags.support, BATTLE_SUPPORT_SET_1..4_* */
} world_unit_t;
typedef char world_unit_size_must_be_296[(sizeof(world_unit_t) == 296) ? 1 : -1];

/* The mode argument of world_formation_build_record_list: which roster units
 * it lists. */
typedef enum world_formation_record_list_mode {
    WORLD_FORMATION_RECORD_LIST_ALL = 0,
    WORLD_FORMATION_RECORD_LIST_AVAILABLE_HUMANS = 1, /* no unit on a proposition, egg or monster */
    WORLD_FORMATION_RECORD_LIST_MONSTERS = 2,         /* no egg, male or female unit */
} world_formation_record_list_mode_e;

/* Result of world_formation_get_unit_lock: whether the unit's
 * equipment and abilities can be changed, and why not. */
typedef enum world_formation_unit_lock {
    WORLD_FORMATION_UNIT_LOCK_MIME = -6, /* a Mime can still learn abilities */
    WORLD_FORMATION_UNIT_LOCK_EGG = -4,
    WORLD_FORMATION_UNIT_LOCK_MONSTER = -3,
    WORLD_FORMATION_UNIT_LOCK_PROPOSITION = -2, /* away on a proposition */
    WORLD_FORMATION_UNIT_UNLOCKED = 1,
} world_formation_unit_lock_e;

/* The formation screen's state: the index of its handler in
 * g_world_formation_menu_handlers. */
typedef enum world_formation_menu {
    WORLD_FORMATION_MENU_UNIT_GRID = 0, /* unit grid and its triangle menu */
    WORLD_FORMATION_MENU_ITEM = 1,      /* item icon list */
    WORLD_FORMATION_MENU_ABILITY = 2,
    WORLD_FORMATION_MENU_JOB_WHEEL = 3,
    WORLD_FORMATION_MENU_DISMISS_UNIT = 4,
    WORLD_FORMATION_MENU_ORDER_UNIT = 5,
    WORLD_FORMATION_MENU_ITEM_EQUIP = 6, /* the item list's rows follow in order */
    WORLD_FORMATION_MENU_ITEM_BEST_FIT = 7,
    WORLD_FORMATION_MENU_ITEM_REMOVE = 8,
    WORLD_FORMATION_MENU_ITEM_LIST = 9,
    WORLD_FORMATION_MENU_ABILITY_SET = 10, /* the ability menu's rows follow in order */
    WORLD_FORMATION_MENU_ABILITY_REMOVE = 11,
    WORLD_FORMATION_MENU_ABILITY_LEARN = 12,
    WORLD_FORMATION_MENU_VIEW_ABILITIES = 13,
    WORLD_FORMATION_MENU_JOB_WHEEL_ENTER = 14,              /* the grid slides out, then the wheel opens */
    WORLD_FORMATION_MENU_JOB_WHEEL_LEAVE = 15,              /* the wheel closes, then the grid returns */
    WORLD_FORMATION_MENU_ITEM_ENTER = 16,                   /* the grid slides out */
    WORLD_FORMATION_MENU_ITEM_LEAVE = 17,                   /* the grid returns */
    WORLD_FORMATION_MENU_ABILITY_ENTER = 18,                /* the grid slides out */
    WORLD_FORMATION_MENU_ABILITY_LEAVE = 19,                /* the grid returns */
    WORLD_FORMATION_MENU_JOB_WHEEL_CLOSING = 20,            /* wheel close animation */
    WORLD_FORMATION_MENU_JOB_WHEEL_OPENING = 21,            /* wheel open animation */
    WORLD_FORMATION_MENU_ABILITY_ENTER_PORTRAIT_SLIDE = 22, /* only the portrait slides; nothing stores it */
    WORLD_FORMATION_MENU_ITEM_ENTER_NO_SLIDE = 23,          /* nothing stores it */
    WORLD_FORMATION_MENU_JOB_CHANGE = 24,                   /* job change flash on the wheel */
} world_formation_menu_e;

/* Provisional: month lengths indexed by 1-based month (0x8018deac; follows
 * the zodiac date limits at 0x8018de84). Read by world_unit_update_monster_breeding. */
extern const u8 g_world_month_lengths[WORLD_MONTH_LENGTH_TABLE_COUNT];
extern const world_zodiac_date_limit_t g_world_zodiac_date_limits[WORLD_ZODIAC_DATE_LIMIT_COUNT];

/* unit */

/* Provisional: one 4-byte breeding entry per monster job from 0x5e
 * (0x8018debc): the hatch-day range and the three offspring jobs by rarity
 * class (world_unit_update_monster_breeding). */
typedef struct world_unit_monster_breed_entry {
    u8 hatch_range; /* 0x00: rand() % hatch_range + 1 (at least 2) egg days */
    u8 offspring_job_by_rarity[WORLD_MONSTER_BREED_RARITY_COUNT]; /* 0x01; common through rare */
} world_unit_monster_breed_entry_t;
typedef char world_unit_monster_breed_entry_size_must_be_4[(sizeof(world_unit_monster_breed_entry_t) == 4) ? 1 : -1];

/* Unpadded unsigned 16-bit x/y/z triple (6 bytes). */
typedef struct vec3u16 {
    u16 x;
    u16 y;
    u16 z;
} vec3u16_t;
typedef char assert_vec3u16_size[sizeof(vec3u16_t) == 6 ? 1 : -1];

void resolve_unit_status_changes(void);
s32 unit_has_adjacent_monster_skill_support(struct unit_stats* unit);
void unit_copy_map_coords_by_runtime_id(u32 runtime_id, SVECTOR* coords);

/* text */
/* Resident text resource indices, used by world_text_get_resident_data and
 * world_text_bind_section_data. */
typedef enum world_text_data {
    WORLD_TEXT_DATA_SKILLSET_NAMES = 0,
    WORLD_TEXT_DATA_JOB_NAMES = 1,
    WORLD_TEXT_DATA_ABILITY_NAMES = 2,
    WORLD_TEXT_DATA_FORMATION_MESSAGES = 3,   /* TEXT_SECTION_FORMATION_MESSAGES */
    WORLD_TEXT_DATA_DISMISS_UNIT_REPLIES = 4, /* WORLD_TEXT_SECTION_FORMATION_TEXT */
    WORLD_TEXT_DATA_JOB_DESCRIPTIONS = 5,     /* WORLD_TEXT_SECTION_FORMATION_TEXT */
    WORLD_TEXT_DATA_NAME_ENTRY_KEYBOARD = 7,
    WORLD_TEXT_DATA_SHOP_DIALOGUE = 8, /* WORLD_TEXT_SECTION_SHOP_DIALOGUE */
} world_text_data_e;

/* WORLD text files materialize 32 section pointers after their 0x80-byte
 * offset/header area. The same table is saved and restored around overlays. */
enum {
    WORLD_TEXT_SECTION_TABLE_BYTES = 128,
    WORLD_TEXT_FILE_HEADER_BYTES = 128,
};

/* The text sections WORLD's screens bind while they are up (text_section_e
 * has the resident ones). */
typedef enum world_text_section {
    WORLD_TEXT_SECTION_TUTORIAL_MESSAGES = 23, /* the running tutorial's message file */
    WORLD_TEXT_SECTION_NAME_ENTRY_KEYS = 28,   /* name entry: the key rows */
    WORLD_TEXT_SECTION_FORMATION_TEXT = 29,    /* formation: what a dismissed unit says, or the job wheel's
                                                * descriptions, whichever was bound last */
    WORLD_TEXT_SECTION_SHOP_MENU_LABELS = 30,
    WORLD_TEXT_SECTION_SHOP_DIALOGUE = 31, /* the shopkeeper's lines */
} world_text_section_e;

extern SVECTOR g_unit_scratch_coords;

/* menu context */
/* Ordinary outfitter screens use a town's world_location_id_e. These values
 * identify the other screens that reuse the formation menu framework. */
typedef enum world_menu_context {
    WORLD_MENU_CONTEXT_NAME_ENTRY = -2,
    WORLD_MENU_CONTEXT_FORMATION = -1,
    WORLD_MENU_CONTEXT_FUR_SHOP = 100,
    WORLD_MENU_CONTEXT_SOLDIER_OFFICE = 101,
} world_menu_context_e;

extern action_command_t g_menu_selected_action;
/* menu */

/* One cell of a directional grid menu: a description text id (base plus
 * offset), the cell index reached by each d-pad direction, and the cursor
 * position. A non-zero `vertical` draws the vertical cursor. */
typedef struct world_menu_grid_cell {
    s16* text_id;
    u16 text_offset;
    u8 next_up;
    u8 next_down;
    u8 next_right;
    u8 next_left;
    point16_t cursor;
    s16 vertical;
} world_menu_grid_cell_t;

/* 0x24-byte menu thread-group record at 0x80189ecc, indexed by
 * the 32-bit request index (0-3). Each group names up to three menu threads. */
typedef struct world_menu_thread_group {
    void (*function[3])(void); /* 0x00; -1 when unused */
    s32 thread_id[3];          /* 0x0c; -1 when unused */
    s32 task_id[3];            /* 0x18 */
} world_menu_thread_group_t;

/* The builder walks the table with a byte offset stepping by 8 (the
 * target's induction variable), so it addresses these fields by offset. */
enum world_menu_line_layout_offset {
    WORLD_MENU_LINE_LAYOUT_X0 = 0,
    WORLD_MENU_LINE_LAYOUT_Y0 = 1,
    WORLD_MENU_LINE_LAYOUT_X1 = 2,
    WORLD_MENU_LINE_LAYOUT_Y1 = 3,
    WORLD_MENU_LINE_LAYOUT_ADD_W0 = 4,
    WORLD_MENU_LINE_LAYOUT_ADD_H0 = 5,
    WORLD_MENU_LINE_LAYOUT_ADD_W1 = 6,
    WORLD_MENU_LINE_LAYOUT_ADD_H1 = 7,
    WORLD_MENU_LINE_LAYOUT_SIZE = 8
};

typedef char world_menu_line_f2_size_must_be_16[(sizeof(LINE_F2) == 16) ? 1 : -1];

/* Partial image view used by 0x801061e0. Upload rectangles determine the
 * second page's size; it begins at byte +0x8000 in both resource layouts. */
typedef struct world_menu_image_pages {
    u32 first_page[8192];
    u32 second_page[1];
} world_menu_image_pages_t;

/* Value range of a numeric menu entry (world_menu_number_entry_thread and
 * world_menu_labeled_number_entry_thread). */
typedef struct world_menu_number_range {
    ranges16_t bounds;       /* 0x00 */
    s16 _unknown_04[2];      /* 0x04: -1 from wldcore_window_init_number_panel_render_thread */
    void (*on_change)(void); /* 0x08 */
} world_menu_number_range_t;

/* Result slots the shop menus share. */
enum {
    WORLD_MENU_SELECTION_SHOP_QUANTITY = 3, /* chosen purchase/sale quantity */
    WORLD_MENU_SELECTION_PROMPT_RESULT = 4, /* yes/no prompt result (0 confirms) */
};

/* Index of a saved cursor in g_menu_selection_values, as the formation
 * and shop menus use them. */
typedef enum world_menu_cursor {
    WORLD_MENU_CURSOR_SHOP_ITEM_TAB = 0,
    WORLD_MENU_CURSOR_EQUIP_SLOT = 2,
    WORLD_MENU_CURSOR_REMOVE_SLOT = 3, /* equipment slot of the remove and sell menus */
    WORLD_MENU_CURSOR_SET_ABILITY_SLOT = 4,
    WORLD_MENU_CURSOR_REMOVE_ABILITY_SLOT = 5,
    WORLD_MENU_CURSOR_ITEM_TAB = 7,
    WORLD_MENU_CURSOR_ABILITY_TAB = 8,
} world_menu_cursor_e;

enum { WORLD_MENU_SCRIPT_END = 0x1c };

/* card */

/* Step of the WORLD memory card screen (world_card_run_menu_screen). */
typedef enum world_card_menu_step {
    WORLD_CARD_MENU_STEP_SELECT_CARD = 0, /* memory card slot 1 or 2 */
    WORLD_CARD_MENU_STEP_SELECT_MODE = 1, /* save or load */
    WORLD_CARD_MENU_STEP_SCANNING = 2,
    WORLD_CARD_MENU_STEP_SELECT_SAVE = 3, /* the save slot list takes input */
    WORLD_CARD_MENU_STEP_LOADING = 4,
    WORLD_CARD_MENU_STEP_SAVING = 5,
    WORLD_CARD_MENU_STEP_LEAVE = 9,   /* starts the fade out */
    WORLD_CARD_MENU_STEP_LEFT = 0xff, /* no step runs while the screen fades */
} world_card_menu_step_e;

/* item */

extern s16 g_gfx_item_icon_clut_base_x; /* item icon CLUT base x */
extern s16 g_gfx_item_icon_clut_base_y; /* item icon CLUT base y */

/* shop */
/* Two-byte shop availability mask, stored most significant byte first: bit
 * 0x8000 >> location id is set when that town's outfitter sells the item. */
typedef struct world_shop_item_availability {
    u8 high;
    u8 low;
} world_shop_item_availability_t;

/* Prefix of the shop roster display stream, through its first number command. */
typedef struct world_shop_roster_count_menu_script {
    menu_draw_state_command_t priority;       /* 0x00 */
    menu_texture_page_command_t texture_page; /* 0x04 */
    menu_fixed_clut_command_t clut;           /* 0x09 */
    menu_sprite_command_t sprites[2];         /* 0x0e */
    menu_draw_number_command_t unit_count;    /* 0x20 */
} world_shop_roster_count_menu_script_t;
typedef char
    world_shop_roster_count_menu_script_size_must_be_42[(sizeof(world_shop_roster_count_menu_script_t) == 42) ? 1 : -1];

/* The shop screen's step: the index of its handler in
 * g_world_shop_menu_step_handlers. The ordinary shop starts at MAIN_MENU, the
 * fur shop at FUR_MENU and the soldier office at SOLDIER_OFFICE_MENU. */
typedef enum world_shop_step {
    WORLD_SHOP_STEP_EXIT = -1, /* world_shop_run_screen leaves its loop */
    WORLD_SHOP_STEP_MAIN_MENU = 0,
    WORLD_SHOP_STEP_BUY_LIST = 1,
    WORLD_SHOP_STEP_BUY_CONFIRM = 2,
    WORLD_SHOP_STEP_BUY_EQUIP = 3, /* who can equip the item under the cursor */
    WORLD_SHOP_STEP_SELL_LIST = 4,
    WORLD_SHOP_STEP_SELL_CONFIRM = 5,
    WORLD_SHOP_STEP_FITTING_ROOM = 6, /* unit grid, then the fitting menu */
    WORLD_SHOP_STEP_FITTING_ROOM_EQUIP = 7,
    WORLD_SHOP_STEP_FITTING_ROOM_BEST_FIT = 8,
    WORLD_SHOP_STEP_FITTING_ROOM_REMOVE = 9,
    WORLD_SHOP_STEP_FITTING_ROOM_SELL_EQUIPPED = 10,
    WORLD_SHOP_STEP_FITTING_ROOM_SELL = 11, /* runs the SELL_LIST and SELL_CONFIRM handlers */
    WORLD_SHOP_STEP_FITTING_ROOM_CHECKOUT = 12,
    WORLD_SHOP_STEP_EXIT_FADE = 13,
    WORLD_SHOP_STEP_FUR_MENU = 14,
    WORLD_SHOP_STEP_FUR_BUY_LIST = 15, /* the shop's poached stock */
    WORLD_SHOP_STEP_FUR_BUY_CONFIRM = 16,
    WORLD_SHOP_STEP_FUR_BUY_EQUIP = 17,
    WORLD_SHOP_STEP_FUR_SELL_LIST = 18, /* the party's inventory */
    WORLD_SHOP_STEP_FUR_SELL_CONFIRM = 19,
    WORLD_SHOP_STEP_SOLDIER_OFFICE_MENU = 20,
    WORLD_SHOP_STEP_HIRE_PREVIEW = 21,
    WORLD_SHOP_STEP_HIRE_CONFIRM = 22, /* declining removes the recruit from the roster */
    WORLD_SHOP_STEP_HIRE_NAME_ENTRY = 23,
    WORLD_SHOP_STEP_HIRE_FEE_CONFIRM = 24,
    WORLD_SHOP_STEP_SOLDIER_OFFICE_REOPEN = 25,
    WORLD_SHOP_STEP_RENAME_SELECT_UNIT = 26,
    WORLD_SHOP_STEP_RENAME_NAME_ENTRY = 27,
} world_shop_step_e;

/* Result of world_shop_run_fitting_room_checkout_prompt. */
typedef enum world_shop_checkout_result {
    WORLD_SHOP_CHECKOUT_DONE = -1,    /* bought, or left without buying: back to the shop's main menu */
    WORLD_SHOP_CHECKOUT_CANCELED = 0, /* back to the fitting room */
    WORLD_SHOP_CHECKOUT_PROMPT_OPEN = 1,
} world_shop_checkout_result_e;

/* Refusals of world_shop_run_item_purchase_quantity_menu and
 * world_shop_run_fur_buy_quantity_menu; otherwise they return whether the
 * quantity prompt is still open. */
enum {
    WORLD_SHOP_QUANTITY_HOLDING_MAX = -2, /* the party already holds 99 */
    WORLD_SHOP_QUANTITY_CANNOT_AFFORD = -1,
};

/* unnamed */
extern s8 D_8018D18A;
extern s8 D_801C833C;
extern u8 D_801CC7D0;
extern u8 D_801CC7D8;
extern u8 D_801CD0EC;
extern u8 D_801CD0F4;

#endif
