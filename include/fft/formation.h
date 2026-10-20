#ifndef FORMATION_H
#define FORMATION_H

/* The Formation screen (the party roster): the unit records it edits, the unit grid, sprites and portraits, the unit
 * banner and panels, Dismiss and Order; the job, ability and item menus have their own headers, and monster breeding
 * (breeding_) sits here too. */

#include "fft/formation_ability.h"
#include "fft/gfx.h"
#include "fft/job.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_script.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "fft/unit_party.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* unit records */
/* formation_unit_t::proposition_status, copied from the party record:
 * the label the formation grid draws below a unit that is away. */
typedef enum formation_unit_proposition {
    FORMATION_UNIT_PROPOSITION_LEAVING = 1,
    FORMATION_UNIT_PROPOSITION_MISSING = 2
} formation_unit_proposition_e;

enum {
    FORMATION_PREVIEW_UNIT_INDEX = PARTY_ROSTER_SLOT_COUNT,
    FORMATION_UNIT_RECORD_COUNT = FORMATION_PREVIEW_UNIT_INDEX + 1
};

/* The 64-byte equipment and ability detail copied to the Formation panels.
 * Slot arrays retain unsigned storage; packed menu-list entries are a separate signed view. */
typedef struct formation_unit_detail {
    s16 move;                     /* 0x00 */
    s16 speed;                    /* 0x02 */
    s16 jump;                     /* 0x04 */
    s16 right_weapon_power;       /* 0x06 */
    s16 left_weapon_power;        /* 0x08 */
    s16 right_weapon_evade;       /* 0x0a */
    s16 left_weapon_evade;        /* 0x0c */
    s16 uses_monster_skillset;    /* 0x0e; primary skillset is in the monster range 0xb0..0xdf */
    s16 two_handed;               /* 0x10; unit_is_two_handing_weapon result */
    s16 physical_attack;          /* 0x12 */
    s16 physical_class_evade;     /* 0x14 */
    s16 physical_shield_evade;    /* 0x16 */
    s16 physical_accessory_evade; /* 0x18 */
    u8 _unused_1a[2];
    s16 magical_attack;                          /* 0x1c */
    s16 magical_class_evade;                     /* 0x1e */
    s16 magical_shield_evade;                    /* 0x20 */
    s16 magical_accessory_evade;                 /* 0x22 */
    u16 equipment[MENU_EQUIPMENT_SLOT_COUNT];    /* 0x24; right hand, left hand, head, body, accessory */
    u16 abilities[FORMATION_ABILITY_SLOT_COUNT]; /* 0x2e */
    u8 _unused_38[8];                            /* 0x38; spare tail of the stat-detail block */
} formation_unit_detail_t;
typedef char formation_unit_detail_size_must_be_64[sizeof(formation_unit_detail_t) == 64 ? 1 : -1];

/* The WORLD menus' record of a unit (g_formation_unit_records), compacted after roster and menu-specific filtering;
 * roster_slot keeps its roster identity. The builder and sorter produce the ordered g_formation_unit_pointers list, not
 * a roster-indexed array. The byte fields from 0x70 on mirror the party_unit_t layout. */
typedef struct formation_unit {
    unit_panel_summary_t status;          /* 0x000; copied whole to the status banner */
    menu_unit_status_identity_t identity; /* 0x022 */
    formation_unit_detail_t detail;       /* 0x030 */
    u8 gender_flags;                      /* 0x070; unit_flags_e (fft/unit.h) */
    u8 _unused_071;
    u8 sprite_set;               /* 0x072; "Sprite Set ID": party_unit_t.sprite_set / character identity */
    u8 equippable_item_types[4]; /* 0x073; bit per item type, read by equipment_item_compatibility_check */
    u8 unlocked_jobs[UNIT_UNLOCKED_JOB_BYTE_COUNT];                                           /* 0x077 */
    u8 learned_abilities[UNIT_LEARNED_ABILITY_JOB_COUNT][UNIT_LEARNED_ABILITY_BYTES_PER_JOB]; /* 0x07a through 0x0b2 */
    u8 job_levels[UNIT_JOB_LEVEL_BYTE_COUNT];                                                 /* 0x0b3 */
    u8 _padding_0bd;                                                                          /* aligns job_points */
    unit_job_points_t job_points; /* 0x0be; current JP followed by total JP */
    u8 name[UNIT_NAME_SIZE];      /* 0x10e; "unit Nickname" */
    u8 monster_graphic;           /* 0x11e; added to formation graphic entry 0x3d when sprite_set == 0x82
                                     (formation_unit_sprite_source_build) */
    u8 proposition_status;        /* 0x11f; "Proposition Byte": formation_unit_proposition_e, 0 when present */
    u16 birthday;                 /* 0x120; unit_stats_t birthday & 0x1ff; egg readers split
                                     it into nibbles */
    u16 egg_color;                /* 0x122; party_unit_t 0xd2; unit_slots_draw loads it as a 16-bit palette
                                     index */
    u8 support_abilities[4];      /* 0x124; copy of unit_stats_t.ability_flags.support, UNIT_SUPPORT_SET_1..4_* */
} formation_unit_t;
typedef char formation_unit_size_must_be_296[(sizeof(formation_unit_t) == 296) ? 1 : -1];

/* The mode argument of formation_record_list_build: which roster units
 * it lists. */
typedef enum formation_record_list_mode {
    FORMATION_RECORD_LIST_ALL = 0,
    FORMATION_RECORD_LIST_AVAILABLE_HUMANS = 1, /* no unit on a proposition, egg or monster */
    FORMATION_RECORD_LIST_MONSTERS = 2          /* no egg, male or female unit */
} formation_record_list_mode_e;

/* Result of formation_unit_lock_get: whether the unit's
 * equipment and abilities can be changed, and why not. */
typedef enum formation_unit_lock {
    FORMATION_UNIT_LOCK_MIME = -6, /* a Mime can still learn abilities */
    FORMATION_UNIT_LOCK_EGG = -4,
    FORMATION_UNIT_LOCK_MONSTER = -3,
    FORMATION_UNIT_LOCK_PROPOSITION = -2, /* away on a proposition */
    FORMATION_UNIT_UNLOCKED = 1
} formation_unit_lock_e;

extern struct formation_unit g_formation_unit_records[FORMATION_UNIT_RECORD_COUNT];
extern formation_unit_t* g_formation_unit_pointers[];
extern s16 g_formation_record_count; /* records in g_formation_unit_records */
extern s16 g_formation_record_order[];
extern struct formation_unit* g_formation_temp_unit;

/* Menu callers cache the builder's return here; rebuilding the record list
 * does not itself refresh this byte. */
extern u8 g_formation_unit_count;

s16 formation_record_list_build(s32 item_id, formation_unit_t** unused_list, formation_record_list_mode_e mode);
void formation_unit_record_build(
    const struct unit_stats* unit, struct formation_unit* record, const struct party_unit* party);
void formation_rebuild_unit_list(void);
s32 formation_roster_slot_find(s16 roster_slot);
void formation_unit_to_temp_copy(s16 index, s32 unused_slot);
void formation_save_records(void);
void formation_sort_roster(s32 index);
s32 formation_unit_lock_get(s32 unit_id);
s32 formation_generate_unit(s16 unit_type);
void formation_unit_name_set(s16 formation_index, const u8* name);
void formation_roster_names_build(void);
void formation_invalid_loadout_remove(s16 unit_id);
s32 formation_inventory_check(s32 mode);

/* formation screen */
/* The formation screen's state: the index of its handler in
 * g_formation_menu_handlers. */
typedef enum formation_menu {
    FORMATION_MENU_UNIT_GRID = 0, /* unit grid and its triangle menu */
    FORMATION_MENU_ITEM = 1,      /* item icon list */
    FORMATION_MENU_ABILITY = 2,
    FORMATION_MENU_JOB_WHEEL = 3,
    FORMATION_MENU_DISMISS_UNIT = 4,
    FORMATION_MENU_ORDER_UNIT = 5,
    FORMATION_MENU_ITEM_EQUIP = 6, /* the item list's rows follow in order */
    FORMATION_MENU_ITEM_BEST_FIT = 7,
    FORMATION_MENU_ITEM_REMOVE = 8,
    FORMATION_MENU_ITEM_LIST = 9,
    FORMATION_MENU_ABILITY_SET = 10, /* the ability menu's rows follow in order */
    FORMATION_MENU_ABILITY_REMOVE = 11,
    FORMATION_MENU_ABILITY_LEARN = 12,
    FORMATION_MENU_VIEW_ABILITIES = 13,
    FORMATION_MENU_JOB_WHEEL_ENTER = 14,            /* the grid slides out, then the wheel opens */
    FORMATION_MENU_JOB_WHEEL_LEAVE = 15,            /* the wheel closes, then the grid returns */
    FORMATION_MENU_ITEM_ENTER = 16,                 /* the grid slides out */
    FORMATION_MENU_ITEM_LEAVE = 17,                 /* the grid returns */
    FORMATION_MENU_ABILITY_ENTER = 18,              /* the grid slides out */
    FORMATION_MENU_ABILITY_LEAVE = 19,              /* the grid returns */
    FORMATION_MENU_JOB_WHEEL_CLOSING = 20,          /* wheel close animation */
    FORMATION_MENU_JOB_WHEEL_OPENING = 21,          /* wheel open animation */
    FORMATION_MENU_ABILITY_ENTER_SPRITE_SLIDE = 22, /* only the sprite slides; nothing stores it */
    FORMATION_MENU_ITEM_ENTER_NO_SLIDE = 23,        /* nothing stores it */
    FORMATION_MENU_JOB_CHANGE = 24                  /* job change flash on the wheel */
} formation_menu_e;

extern s32 g_formation_current_menu; /* formation_menu_e */
extern u8 g_formation_screen_running;
extern u32* g_formation_ordering_table;
extern u8 g_formation_menu_text_table[];
extern u8 g_formation_menu_text_buffer[];
extern menu_entry_t g_formation_triangle_menu;
extern u8 g_formation_triangle_menu_open;
extern u8 g_formation_triangle_menu_running;

/* Set by WORLD's formation screen each frame and cleared by the menu input update; WORLD's dialog thread
 * treats Help boxes differently while it is set. */
extern s32 g_formation_screen_active;

void formation_screen_run(s32 tutorial, s32 unused);
void formation_tutorial_run(s32 tutorial_index);
void formation_menu_state_init(void);
void formation_display_init(void);
void formation_world_map_return_run(void);
void formation_backdrop_otag_set(u32* ordering_table);
void formation_display_mode_set(s32 mode);
void formation_background_tiles_draw(s32 y, s32 unused_1);
void formation_zodiac_backdrop_run(void);

/* unit grid */
extern u8 g_formation_cursor_ot_override;
extern s8 g_formation_selection_cursor_ready;
extern s8 g_formation_scroll_position;
extern s8 g_formation_scroll_velocity;
extern u8 g_formation_unit_cycle_mode;
extern u8 g_formation_unit_browse_enabled;
extern rgb24_t g_formation_dimmed_sprite_color;

/* get_mode and get_shade take the unit slot index; NULL selects the default. */
s32 formation_grid_update(s16 background_y, s16 toggle_banner, s16 allow_browse, s16 input, s16 locked,
    s32 (*get_mode)(s32), s32 (*get_shade)(s32));
void formation_selection_update(void);
void formation_selected_unit_cycle(s16 pad_buttons);
void formation_grid_cursor_init(s32 formation_index);
void formation_selection_cursor_init(s16 formation_index);
s32 formation_grid_scroll_check(void);
s32 formation_grid_transition_update(s32 mode);
s32 formation_grid_return_update(s32 mode);

/* unit sprites and portraits */
extern u8 g_formation_monster_sprite_table[20]; /* indexed by sprite code - 0x3f */
extern u8 g_formation_sprite_tall_cell[];

void formation_prepare_sprites(void);
void formation_build_sprites(void);
s32 formation_unit_palette_index_get(s32 roster_slot);
void formation_unit_sprite_source_build(s16 formation_index, texture_source_t* destination, s32 unused);
void formation_job_sprite_source_build(s32 job_id, s16 is_female, texture_source_t* destination);
void formation_unit_sprite_draw(s16 unit_index, point16_t* pos, rgb24_t* rgb, s32 ot_index);
void formation_sprite_with_shadow_draw(const oriented_quad_t* pos, const rgb24_t* rgb, s32 ot_index);
void formation_sprite_at_rest_draw(s16 unit_index);
u8 formation_sprite_slide_update(void);

/* unit banner and stat bar */
extern u8 g_formation_unit_banner_enabled;
extern u8 g_formation_saved_banner_enabled;
extern s8 g_formation_banner_slide_step;
extern u8 g_formation_banner_active;

/* Set while the selected unit is in the upper rows of the grid: the unit banner
 * then sits at the bottom of the screen and the stat bar at the top (BUNIT's
 * g_unit_list_status_banner_at_bottom). The latch is the value the banner was last
 * placed for. Both bytes use the shared unsigned declaration in every reader. */
extern u8 g_formation_unit_banner_at_bottom;
extern u8 g_formation_unit_banner_at_bottom_latch;

s32 formation_banner_countdown(s32 message_id, s32 frames);

/* unit panels */
/* The status panels of the selected unit and of the unit it is compared with, and the stat preview panel. */
extern menu_unit_status_identity_t g_formation_selected_unit_identity;
extern struct unit_panel_record g_formation_selected_unit_stat_detail;
extern unit_panel_summary_t g_formation_comparison_unit_stat_summary;
extern menu_unit_status_identity_t g_formation_comparison_unit_identity;
extern unit_panel_thread_t g_formation_stat_preview_panel_thread_params;
extern s32 g_formation_preview_stats_window_active;
extern unit_panel_thread_t g_formation_change_banner_thread_params;
extern unit_panel_thread_t g_formation_equipment_panel_thread_params;

void formation_stage_selected_unit(void);
void formation_selection_stage_update(void);
void formation_selected_stats_refresh(void);
s32 formation_selected_unit_name_id_get(void);
void formation_redraw_panels(void);
void formation_panels_stop(void);
void formation_toggle_preview_stats_window(s32 flag);
void formation_toggle_stat_preview_panel(s32 enable);
void formation_toggle_change_panel(s32 enable);
void formation_toggle_ability_panel(s32 enable);
void formation_equipment_panel_open(s32 enabled);

/* dismissal */
extern s16 g_formation_dismiss_anim_timer;
extern s8 g_formation_dismiss_prompt_running;

s32 formation_can_dismiss(s16 formation_idx);
void formation_dismiss(s16 formation_idx);
void formation_dismiss_menu_update(void);

/* order */
/* Order Unit keys select both a record field and its menu text. The gaps are
 * not selectable criteria. Number sorts by roster slot ascending; the others
 * sort descending. */
typedef enum formation_sort_key {
    FORMATION_SORT_KEY_NUMBER = 0,
    FORMATION_SORT_KEY_LEVEL = 1,
    FORMATION_SORT_KEY_JOB = 2,
    FORMATION_SORT_KEY_BRAVE = 4,
    FORMATION_SORT_KEY_FAITH = 5,
    FORMATION_SORT_KEY_MAX_HP = 7,
    FORMATION_SORT_KEY_MAX_MP = 9
} formation_sort_key_e;

enum { FORMATION_SORT_KEY_COUNT = 7 };

extern s8 g_formation_order_open;

void formation_order_menu_update(void);

/* monster breeding */
void breeding_update(s32 unused);

#endif
