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
typedef union formation_abilities {
    formation_ability_fields_t fields;
    u16 slots[5];
} formation_abilities_t;
typedef char formation_abilities_size_must_be_10[(sizeof(formation_abilities_t) == 10) ? 1 : -1];

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

/* The WORLD menus' record of a unit (g_formation_unit_records), compacted after roster and menu-specific filtering;
 * roster_slot keeps its roster identity. The builder and sorter produce the ordered g_formation_unit_pointers list, not
 * a roster-indexed array. The byte fields from 0x70 on mirror the party_unit_t layout. */
typedef struct formation_unit {
    unit_panel_summary_t status;          /* 0x000; copied whole to the status banner */
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
    formation_abilities_t abilities;          /* 0x05e */
    u8 _unused_068[8];                        /* 0x068; spare tail of the stat-detail block */
    u8 gender_flags;                          /* 0x070; unit_flags_e (fft/unit.h) */
    u8 _unused_071;
    u8 sprite_set;               /* 0x072; "Sprite Set ID": party_unit_t.sprite_set / character identity */
    u8 equippable_item_types[4]; /* 0x073; bit per item type, read by equipment_can_equip_item */
    u8 unlocked_jobs[UNIT_UNLOCKED_JOB_BYTE_COUNT];                                           /* 0x077 */
    u8 learned_abilities[UNIT_LEARNED_ABILITY_JOB_COUNT][UNIT_LEARNED_ABILITY_BYTES_PER_JOB]; /* 0x07a through 0x0b2 */
    u8 job_levels[UNIT_JOB_LEVEL_BYTE_COUNT];                                                 /* 0x0b3 */
    u8 _padding_0bd;                                                                          /* aligns job_points */
    unit_job_points_t job_points; /* 0x0be; current JP followed by total JP */
    u8 name[UNIT_NAME_SIZE];      /* 0x10e; "unit Nickname" */
    u8 monster_graphic;           /* 0x11e; added to formation graphic entry 0x3d when sprite_set == 0x82
                                     (formation_build_unit_sprite_source) */
    u8 proposition_status;        /* 0x11f; "Proposition Byte": formation_unit_proposition_e, 0 when present */
    u16 birthday;                 /* 0x120; unit_stats_t birthday & 0x1ff; egg readers split
                                     it into nibbles */
    u16 egg_color;                /* 0x122; party_unit_t 0xd2; formation_draw_unit_slots loads it as a 16-bit palette
                                     index */
    u8 support_abilities[4];      /* 0x124; copy of unit_stats_t.ability_flags.support, UNIT_SUPPORT_SET_1..4_* */
} formation_unit_t;
typedef char formation_unit_size_must_be_296[(sizeof(formation_unit_t) == 296) ? 1 : -1];

/* The mode argument of formation_build_record_list: which roster units
 * it lists. */
typedef enum formation_record_list_mode {
    FORMATION_RECORD_LIST_ALL = 0,
    FORMATION_RECORD_LIST_AVAILABLE_HUMANS = 1, /* no unit on a proposition, egg or monster */
    FORMATION_RECORD_LIST_MONSTERS = 2          /* no egg, male or female unit */
} formation_record_list_mode_e;

/* Result of formation_get_unit_lock: whether the unit's
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

s16 formation_build_record_list(s32 item_id, formation_unit_t** unused_list, s32 mode);
void formation_build_unit_record(
    const struct unit_stats* unit, struct formation_unit* record, const struct party_unit* party);
void formation_rebuild_unit_list(void);
s32 formation_find_by_roster_slot(s16 roster_slot);
void formation_copy_unit_to_temp(s16 index, s32 unused_slot);
void formation_save_records(void);
void formation_sort_roster(s32 index);
s32 formation_get_unit_lock(s32 unit_id);
s32 formation_generate_unit(s16 unit_type);
void formation_set_unit_name(s16 formation_index, const u8* name);
void formation_build_roster_names(void);
void formation_remove_invalid_loadout(s16 unit_id);
s32 formation_has_items(s32 mode);

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

void formation_run_screen(s32 tutorial, s32 unused);
void formation_run_tutorial(s32 tutorial_index);
void formation_init_menu_state(void);
void formation_init_display(void);
void formation_reopen_after_world_map(void);
void formation_set_backdrop_otag(u32* ordering_table);
void formation_set_display_mode(s32 mode);
void formation_draw_background_tiles(s32 y, s32 unused_1);
void formation_zodiac_backdrop_thread(void);

/* unit grid */
extern u8 g_formation_cursor_ot_override;
extern s8 g_formation_selection_cursor_ready;
extern s8 g_formation_scroll_position;
extern s8 g_formation_scroll_velocity;
extern u8 g_formation_unit_cycle_mode;
extern u8 g_formation_unit_browse_enabled;
extern rgb24_t g_formation_dimmed_sprite_color;

/* get_mode and get_shade take the unit slot index; NULL selects the default. */
s32 formation_update_grid(s16 background_y, s16 toggle_banner, s16 allow_browse, s16 input, s16 locked,
    s32 (*get_mode)(s32), s32 (*get_shade)(s32));
void formation_update_selection(void);
void formation_cycle_selected_unit(s16 pad_buttons);
void formation_init_grid_cursor(s32 formation_index);
void formation_init_selection_cursor(s16 formation_index);
s32 formation_is_grid_scroll_needed(void);
s32 formation_step_grid_transition(s32 mode);
s32 formation_step_grid_return(s32 mode);

/* unit sprites and portraits */
extern u8 g_formation_monster_sprite_table[20]; /* indexed by sprite code - 0x3f */
extern u8 g_formation_sprite_tall_cell[];

void formation_prepare_sprites(void);
void formation_build_sprites(void);
s32 formation_get_unit_palette_index(s32 roster_slot);
void formation_build_unit_sprite_source(s16 formation_index, texture_source_t* destination, s32 unused);
void formation_build_job_sprite_source(s32 job_id, s16 is_female, texture_source_t* destination);
void formation_draw_unit_sprite(s16 unit_index, point16_t* pos, rgb24_t* rgb, s32 ot_index);
void formation_draw_sprite_with_shadow(const oriented_quad_t* pos, const rgb24_t* rgb, s32 ot_index);
void formation_draw_sprite_at_rest(s16 unit_index);
u8 formation_update_sprite_slide(void);

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
void formation_restage_on_selection_change(void);
void formation_recalculate_selected_stats(void);
s32 formation_get_selected_unit_name_id(void);
void formation_redraw_panels(void);
void formation_stop_panels(void);
void formation_toggle_preview_stats_window(s32 flag);
void formation_toggle_stat_preview_panel(s32 enable);
void formation_toggle_change_panel(s32 enable);
void formation_toggle_ability_panel(s32 enable);
void formation_open_equipment_panel(s32 enabled);

/* dismissal */
extern s16 g_formation_dismiss_anim_timer;
extern s8 g_formation_dismiss_prompt_running;

s32 formation_can_dismiss(s16 formation_idx);
void formation_dismiss(s16 formation_idx);
void formation_dismiss_menu(void);

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

void formation_order_menu(void);

/* monster breeding */
void breeding_update(s32 unused);

#endif
