#ifndef WORLD_H
#define WORLD_H

/* WORLD.BIN (formation, shops, memory card and menu screens): the types and declarations its topic headers share. */

#include "fft/data.h"
#include "fft/gfx.h"
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

/* tutorial */

/* Serialized tutorial commands; opcode selects the operand record. */
typedef struct world_tutorial_message_instruction {
    u8 opcode;
    u8 text_id_le[2];
    u8 y_offset;
} world_tutorial_message_instruction_t;

typedef struct world_tutorial_dialog_instruction {
    u8 opcode;
    u8 text_id_le[2];
} world_tutorial_dialog_instruction_t;

typedef struct world_tutorial_circle_instruction {
    u8 opcode;
    u8 x;
    u8 y;
    u8 radius;
    u8 scale_x;
    u8 scale_y;
} world_tutorial_circle_instruction_t;

/* formation */

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

/* Provisional: month lengths indexed by 1-based month (0x8018deac; follows
 * the zodiac date limits at 0x8018de84). Read by world_unit_update_monster_breeding. */
extern const u8 g_world_month_lengths[WORLD_MONTH_LENGTH_TABLE_COUNT];
extern const world_zodiac_date_limit_t g_world_zodiac_date_limits[WORLD_ZODIAC_DATE_LIMIT_COUNT];

/* unit */

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

extern SVECTOR g_unit_scratch_coords;

/* menu context */

extern action_command_t g_menu_selected_action;
/* menu */

typedef char world_menu_line_f2_size_must_be_16[(sizeof(LINE_F2) == 16) ? 1 : -1];

/* Value range of a numeric menu entry (world_menu_number_entry_thread and
 * world_menu_labeled_number_entry_thread). */
typedef struct world_menu_number_range {
    ranges16_t bounds;       /* 0x00 */
    s16 _unknown_04[2];      /* 0x04: -1 from wldcore_window_init_number_panel_render_thread */
    void (*on_change)(void); /* 0x08 */
} world_menu_number_range_t;

/* item */

extern s16 g_gfx_item_icon_clut_base_x; /* item icon CLUT base x */
extern s16 g_gfx_item_icon_clut_base_y; /* item icon CLUT base y */

/* shop */

/* Prefix of the shop roster display stream, through its first number command. */
typedef struct world_shop_roster_count_menu_script {
    menu_draw_state_command_t priority;       /* 0x00 */
    menu_texture_page_command_t texture_page; /* 0x04 */
    menu_fixed_clut_command_t clut;           /* 0x09 */
    menu_sprite_command_t sprites[2];         /* 0x0e */
    menu_draw_number_command_t unit_count;    /* 0x20 */
} world_shop_roster_count_menu_script_t;

/* unnamed */
extern s8 g_world_unknown_8018d18a; /* cleared when Set Ability opens; never read */
extern s8 g_world_unknown_801c833c; /* cleared with the formation scroll velocity; never read */
extern u8 g_world_unknown_801cc7d0; /* cleared with the card load and save failure flags; never read */
extern u8 g_world_unknown_801cc7d8; /* as g_world_unknown_801cc7d0 */
extern u8 g_world_unknown_801cd0ec; /* cleared when the Soldier Office hire menu opens; never read */
extern u8 g_world_unknown_801cd0f4; /* as g_world_unknown_801cd0ec */

#endif
