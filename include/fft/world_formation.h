#ifndef WORLD_FORMATION_H
#define WORLD_FORMATION_H

/* WORLD's formation screen: the unit records it edits, the unit grid, sprites and banners, dismissal, the job wheel,
 * the ability, equipment and item menus, and monster breeding. */

#include "fft/data.h"
#include "fft/data_job.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_panel.h"
#include "fft/menu_script.h"
#include "fft/menu_status.h"
#include "fft/unit.h"
#include "fft/unit_party.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* unit records */
/* Formation ability slots are selected by index in the equipment menu. */
typedef struct formation_ability_fields {
    u16 primary_skillset;   /* 0x00 */
    u16 secondary_skillset; /* 0x02 */
    s16 reaction_ability;   /* 0x04 */
    u16 support_ability;    /* 0x06 */
    s16 movement_ability;   /* 0x08 */
} formation_ability_fields_t;

typedef union formation_abilities {
    formation_ability_fields_t fields;
    u16 slots[5];
} formation_abilities_t;
typedef char world_formation_abilities_size_must_be_10[(sizeof(formation_abilities_t) == 10) ? 1 : -1];

/* formation_unit_t::proposition_status, copied from the party record:
 * the label the formation grid draws below a unit that is away. */
typedef enum formation_unit_proposition {
    FORMATION_UNIT_PROPOSITION_LEAVING = 1,
    FORMATION_UNIT_PROPOSITION_MISSING = 2,
} formation_unit_proposition_e;

enum {
    FORMATION_PREVIEW_UNIT_INDEX = PARTY_ROSTER_SLOT_COUNT,
    FORMATION_UNIT_RECORD_COUNT = FORMATION_PREVIEW_UNIT_INDEX + 1,
};

/* The WORLD menus' record of a unit (g_formation_unit_records), compacted after roster and menu-specific filtering;
 * roster_slot keeps its roster identity. The builder and sorter produce the ordered g_formation_unit_pointers list, not
 * a roster-indexed array. The byte fields from 0x70 on mirror the party_unit_t layout. Magical shield and accessory
 * evade are provisionally placed at 0x50/0x52, after the physical fields, to follow the 16-bit sequence. */
typedef struct formation_unit {
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
                                     (formation_build_unit_graphic) */
    u8 proposition_status;        /* 0x11f; "Proposition Byte": formation_unit_proposition_e, 0 when present */
    u16 birthday;                 /* 0x120; unit_stats_t birthday & 0x1ff; egg readers split
                                     it into nibbles */
    u16 egg_color;                /* 0x122; party_unit_t 0xd2; formation_draw_unit_slots loads it as a 16-bit palette
                                     index */
    u8 support_abilities[4];      /* 0x124; copy of unit_stats_t.ability_flags.support, BATTLE_SUPPORT_SET_1..4_* */
} formation_unit_t;
typedef char world_unit_size_must_be_296[(sizeof(formation_unit_t) == 296) ? 1 : -1];

/* The mode argument of formation_build_record_list: which roster units
 * it lists. */
typedef enum formation_record_list_mode {
    FORMATION_RECORD_LIST_ALL = 0,
    FORMATION_RECORD_LIST_AVAILABLE_HUMANS = 1, /* no unit on a proposition, egg or monster */
    FORMATION_RECORD_LIST_MONSTERS = 2,         /* no egg, male or female unit */
} formation_record_list_mode_e;

/* Result of formation_get_unit_lock: whether the unit's
 * equipment and abilities can be changed, and why not. */
typedef enum formation_unit_lock {
    FORMATION_UNIT_LOCK_MIME = -6, /* a Mime can still learn abilities */
    FORMATION_UNIT_LOCK_EGG = -4,
    FORMATION_UNIT_LOCK_MONSTER = -3,
    FORMATION_UNIT_LOCK_PROPOSITION = -2, /* away on a proposition */
    FORMATION_UNIT_UNLOCKED = 1,
} formation_unit_lock_e;

/* Command texts of the formation's Item and Ability menus, chosen by the
 * selected unit's formation_unit_lock_e. */
enum {
    FORMATION_ITEM_MENU_TEXT_UNLOCKED = 0xd801,
    FORMATION_ITEM_MENU_TEXT_LOCKED = 0xd802,
    FORMATION_ABILITY_MENU_TEXT_UNLOCKED = 0xd80b,
    FORMATION_ABILITY_MENU_TEXT_LOCKED = 0xd80c,
    FORMATION_ABILITY_MENU_TEXT_MIME = 0xd80d,
};

extern struct formation_unit g_formation_unit_records[FORMATION_UNIT_RECORD_COUNT];
extern formation_unit_t* g_formation_unit_pointers[];
extern s16 g_formation_record_count; /* records in g_formation_unit_records */
extern s16 g_formation_record_order[];
/* Menu callers cache the builder's return here; rebuilding the record list
 * does not itself refresh this byte. */
extern u8 g_formation_unit_count;
extern struct formation_unit* g_formation_temp_unit;
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
s32 formation_job_change(s16 unit_id, s16 job_id, s32 update_equipment);
s32 formation_ability_set_slot(s16 unit_id, s16 slot, s16 ability, s32 update_equipment);
s32 formation_item_can_equip_in_slot(s16 unit_index, s16 slot, s16 item_id);
s32 formation_item_equip(s16 unit, s16 slot, s16 item_id);
void formation_remove_invalid_loadout(s16 unit_id);
void formation_recalculate_selected_stats(void);

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
    FORMATION_MENU_JOB_WHEEL_ENTER = 14,              /* the grid slides out, then the wheel opens */
    FORMATION_MENU_JOB_WHEEL_LEAVE = 15,              /* the wheel closes, then the grid returns */
    FORMATION_MENU_ITEM_ENTER = 16,                   /* the grid slides out */
    FORMATION_MENU_ITEM_LEAVE = 17,                   /* the grid returns */
    FORMATION_MENU_ABILITY_ENTER = 18,                /* the grid slides out */
    FORMATION_MENU_ABILITY_LEAVE = 19,                /* the grid returns */
    FORMATION_MENU_JOB_WHEEL_CLOSING = 20,            /* wheel close animation */
    FORMATION_MENU_JOB_WHEEL_OPENING = 21,            /* wheel open animation */
    FORMATION_MENU_ABILITY_ENTER_PORTRAIT_SLIDE = 22, /* only the portrait slides; nothing stores it */
    FORMATION_MENU_ITEM_ENTER_NO_SLIDE = 23,          /* nothing stores it */
    FORMATION_MENU_JOB_CHANGE = 24,                   /* job change flash on the wheel */
} formation_menu_e;

/* Screen x and texture u of one background tile; two alternating rows of four. */
typedef struct formation_background_tile {
    u16 x;
    u16 u;
} formation_background_tile_t;
typedef char world_formation_background_tile_size_must_be_4[(sizeof(formation_background_tile_t) == 4) ? 1 : -1];

extern s32 g_formation_current_menu; /* formation_menu_e */
extern void (*g_formation_menu_handlers[])(void);
extern s16 g_formation_open_request_state; /* world_map_request_state_e */
extern u8 g_formation_screen_running;
extern s8 g_formation_main_menu_initialized;
extern menu_input_state_t g_screen_input_state;
/* Alias for the first 32-bit field retained for the target's separate absolute store. */
extern s32 g_screen_new_buttons;
extern u32 g_screen_allowed_buttons; /* pad mask applied to the new presses of each menu frame */
extern u32* g_formation_ordering_table;
/* g_main_system_flags as saved by world_map_hide (main_system_flags_e). */
extern POLY_FT4 g_formation_view_quads[2][30];
extern formation_background_tile_t g_formation_background_tiles[2][4];
extern u8 g_formation_menu_text_table[];
extern u8 g_formation_menu_text_buffer[];
extern clut_t g_formation_menu_palettes;
extern menu_entry_t g_formation_triangle_menu;
extern u8 g_formation_triangle_menu_open;
extern u8 g_formation_triangle_menu_running;
extern menu_entry_t g_formation_egg_triangle_menu;
/* Chained after the egg triangle menu; formation_grid_menu only resets it. */
extern menu_entry_t g_formation_egg_submenu;
void formation_run_screen(s32 tutorial, s32 unused);
void formation_run_at_menu(s32 menu_index);
s16 screen_menu_frame(u32* otag, u32 input);
void formation_init_menu_state(void);
void formation_init_display(void);
void screen_reset_context(void);
void formation_reopen_after_world_map(void);
void formation_stop_panels(void);
void screen_set_allowed_buttons(u32 mask);
void formation_set_backdrop_otag(u32* ordering_table);
void formation_set_display_mode(s32 mode);
void formation_draw_background_tiles(s32 y, s32 unused_1);
void formation_zodiac_backdrop_thread(void);

/* unit grid */
extern oriented_quad_t g_formation_cursor_sprite;
extern oriented_quad_t g_formation_cursor_shadow_sprite;
extern point16_t g_formation_cursor_position;
extern point16_t g_formation_cursor_target;
/* Cursor trail position history, reset by formation_reset_slots_and_frame_tpages; entry 7 is the newest and
 * x is -1 in an unused entry. */
extern point16_t g_formation_cursor_trail[8];
extern u8 g_formation_cursor_trail_brightness[8];
extern u8 g_formation_cursor_ot_override;
extern s8 g_formation_selection_cursor_ready;
extern s8 g_formation_scroll_position;
extern s8 g_formation_scroll_velocity;
/* Cleared with the formation scroll velocity; nothing reads it. */
extern s8 g_formation_scroll_unread; /* cleared with the formation scroll velocity; never read */
extern u8 g_formation_unit_cycle_mode;
extern u8 g_formation_unit_browse_enabled;
extern s16 g_formation_selected_unit_index_latch;
extern oriented_quad_t g_formation_unit_dot_sprite;
extern s8 g_formation_dot_glow_timer;
extern s8 g_formation_dot_glow_direction;
/* Three consecutive 20-byte sprite records. */
extern oriented_quad_t g_formation_shadow_egg_sprites[3];
extern rgb24_t g_formation_away_unit_color;
extern rgb24_t g_formation_saved_sprite_color;
extern rgb24_t g_formation_dimmed_sprite_color;
extern rgb24_t g_formation_normal_sprite_color;
extern oriented_quad_t g_formation_label_cant_equip;
extern oriented_quad_t g_formation_label_equipping;
extern oriented_quad_t g_formation_label_leaving;
extern oriented_quad_t g_formation_label_missing;
extern menu_sprite_cell_t g_formation_label_tiles[];
extern menu_sprite_cell_t g_formation_label_tiles_bottom[];
extern u8 g_formation_grid_transition_frame;
extern u8 g_formation_grid_return_frame;
extern u8 g_formation_grid_return_done;
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
extern texture_source_t g_formation_graphic_entries[];
extern u8 g_formation_generic_human_graphic_index_table[];
/* Portrait code per sprite set: low byte is the portrait index. */
extern u8 g_formation_unit_slide_pending;
extern volatile u8 g_formation_unit_slide_frame;
extern u8 g_formation_portrait_slide_frame;
s32 formation_get_party_unit_sprite(s32 party_index);
s32 formation_get_unit_palette_index(s32 roster_slot);
void formation_build_unit_graphic(s16 formation_index, texture_source_t* destination, s32 unused);
void formation_get_generic_human_graphic(s32 job_id, s16 is_female, texture_source_t* destination);
void formation_draw_unit_sprite(s16 unit_index, point16_t* pos, rgb24_t* rgb, s32 ot_index);
void formation_draw_sprite_with_shadow(const oriented_quad_t* pos, const rgb24_t* rgb, s32 ot_index);
void formation_place_portrait_at_rest(s16 unit_index);
u8 formation_step_portrait_slide(void);
void formation_stage_selected_unit(void);

/* unit banner and stat bar */
/* Set while the selected unit is in the upper rows of the grid: the unit banner
 * then sits at the bottom of the screen and the stat bar at the top (BUNIT's
 * g_bunit_status_banner_at_bottom). The latch is the value the banner was last
 * placed for. Majority spelling: five readers sign-extend as u8. Two files
 * (formation_init_selection_cursor, formation_step_grid_transition)
 * need s8 to match and keep their own local extern. */
extern u8 g_formation_unit_banner_at_bottom;
extern u8 g_formation_unit_banner_at_bottom_latch;
extern u8 g_formation_unit_banner_enabled;
extern u8 g_formation_saved_banner_enabled;
extern s8 g_formation_banner_slide_step;
extern u8 g_formation_stat_display_mode; /* menu_unit_readout_e, HP to BRAVE_FAITH */
extern menu_instruction_t g_formation_stat_bar_window_top;
extern menu_instruction_t g_formation_stat_bar_window_bottom;
extern menu_draw_number_command_t g_formation_gauge_value_number;
extern menu_draw_number_command_t g_formation_gauge_max_number;
extern menu_draw_number_command_t g_formation_level_number;
extern menu_draw_number_command_t g_formation_experience_number;
extern menu_draw_number_command_t g_formation_brave_number;
extern menu_draw_number_command_t g_formation_faith_number;
extern menu_draw_number_command_t g_formation_brave_number_wide;
extern menu_draw_number_command_t g_formation_faith_number_wide;
extern s8 g_formation_gauge_bar_offsets[4];
extern rgb24_t g_formation_hp_gauge_colors[4];
extern rgb24_t g_formation_mp_gauge_colors[4];
extern rgb24_t g_formation_ct_gauge_colors[4];

/* unit panels */
extern CVECTOR g_formation_panel_bar_colors[];
extern image_load_parameters_t g_formation_summary_label_sprite_params[];
extern image_load_parameters_t g_formation_summary_value_sprite_params[];
extern RECT g_formation_status_panel_origin;
extern image_load_parameters_t g_formation_status_sprite_params[];
extern image_load_parameters_t g_formation_status_portrait_params;
extern u8 g_formation_banner_active;
extern menu_entry_t g_formation_banner_menu;
extern s8 g_formation_banner_state; /* banner state: 0 idle, 1 finishing, else countdown */
s32 unit_facing_get_sixteenth_by_runtime_id(s32 runtime_id);

/* dismissal */
/* An entry of the sprite-set to script-variable table the dismissal tail walks (g_formation_dismiss_entries),
 * terminated by sprite_set == -1. The four-byte stride is proved by the target advancing both its record pointer and
 * its byte-offset index by 4 per iteration. */
typedef struct formation_dismiss_entry {
    s16 sprite_set;  /* 0x00 */
    s16 variable_id; /* 0x02 */
} formation_dismiss_entry_t;

extern formation_dismiss_entry_t g_formation_dismiss_entries[];
extern menu_entry_t g_formation_dismiss_prompt;
extern s16 g_formation_dismiss_prompt_text_id;
extern s16 g_formation_dismiss_prompt_kind;
extern menu_entry_t g_formation_dismiss_confirm_menu[4];
extern menu_entry_t g_formation_dismiss_confirm_menu_scrolled[4];
extern menu_entry_t* g_formation_dismiss_menu_data;
extern u8 g_formation_dismiss_step;
extern s16 g_formation_dismiss_anim_timer;
extern s8 g_formation_dismiss_prompt_running;
s32 formation_can_dismiss(s16 formation_idx);
void formation_dismiss(s16 formation_idx);
void formation_dismiss_menu(void);

/* order and best fit */
extern menu_entry_t g_formation_order_menu;
extern u8 g_formation_order_saved_browse_enabled;
extern menu_entry_t g_formation_item_best_fit_confirm_menu;
void formation_item_build_best_fit(s16 unit_index, s16* equipment);
s32 equipment_can_equip_item(s16 unit_index, s16 item_id);

/* job wheel and learning */
/* Result of formation_job_step_change, kept in
 * g_formation_job_change_anim_state: the phase of the job-change dissolve. */
typedef enum formation_job_change_anim {
    FORMATION_JOB_CHANGE_ANIM_DONE = 0,
    FORMATION_JOB_CHANGE_ANIM_DISSOLVE = 1,
    FORMATION_JOB_CHANGE_ANIM_CLOSING = 2, /* the last 20 frames */
} formation_job_change_anim_e;

/* An entry of the job list job_list_build writes. */
enum {
    FORMATION_JOB_LIST_ENTRY_ID_MASK = 0x3ff,
    /* Set in mode INCLUDE_OTHER_UNIT_UNLOCKS on the unit's current job and on
     * a job only another unit has unlocked: the job wheel shows the entry
     * but does not change to it. */
    FORMATION_JOB_LIST_ENTRY_UNAVAILABLE = 0x4000,
};

/* The one mode job_list_build tests; its callers also pass 0 and 1. */
enum { FORMATION_JOB_LIST_MODE_INCLUDE_OTHER_UNIT_UNLOCKS = 2 };

extern s8 g_formation_job_wheel_initialized;
extern s16 g_formation_job_wheel_unit_index;
extern s16 g_formation_job_wheel_jobs[];
extern s16 g_formation_job_wheel_mastered_jobs[];
extern s16 g_formation_job_wheel_job_count;
extern s16 g_formation_job_wheel_cursor_index;
extern u8 g_formation_job_wheel_job_selected;
extern u8 g_formation_job_wheel_saved_browse_enabled;
extern menu_instruction_t g_formation_job_wheel_script;
extern s16 g_formation_job_wheel_name_text;
extern RECT g_formation_job_wheel_name_rect;
/* The current and the new job's portrait; separate objects, since
 * formation_job_wheel_menu addresses each with its own lui/addiu. */
extern textured_rect_source_t g_formation_job_wheel_portrait_quad;
extern textured_rect_source_t g_formation_job_wheel_new_portrait_quad;
extern oriented_quad_t g_formation_job_wheel_mastered_icon_sprite;
extern point16_t g_formation_job_wheel_unit_sprite_position;
extern u8 g_formation_job_wheel_center_x;
extern u8 g_formation_job_wheel_center_y;
extern u8 g_formation_job_wheel_animation_counter;
extern s8 g_formation_job_wheel_spin_progress;
extern s8 g_formation_job_wheel_spin_direction;
extern u8 g_formation_job_wheel_opening;
extern u8 g_formation_job_wheel_open_frame;
extern u8 g_formation_job_wheel_closing;
extern u8 g_formation_job_wheel_close_frame;
extern u8 g_formation_job_change_active;
extern s8 g_formation_job_change_anim_state; /* formation_job_change_anim_e */
extern u8 g_formation_job_change_confirmed;
extern s16 g_formation_job_change_skip_requested;
extern s16 g_formation_job_change_frame;
extern rgb24_t g_formation_job_change_wave_colors[3];
extern rgb24_t g_formation_job_change_flash_colors[4];
extern s16* g_formation_job_change_dissolve_order;
extern RECT g_formation_job_change_old_sprite_rect;
extern RECT g_formation_job_change_new_sprite_rect;
extern u8* g_formation_job_change_old_sprite_image;
extern u8* g_formation_job_change_new_sprite_image;
extern u8* g_formation_job_change_new_sprite_source;
extern s32 g_formation_ability_learn_job_total_jp;
extern s8 g_formation_ability_learn_menu_initialized;
extern s8 g_formation_ability_learn_menu_step; /* menu_job_list_step_e */
extern s8 g_formation_ability_learn_job_list_initialized;
extern menu_instruction_t g_formation_ability_learn_job_list_script;
extern s16 g_formation_ability_learn_job_name_list[];
extern s8 g_formation_ability_learn_ability_list_open;
extern s16 g_formation_ability_learn_selected_job;
extern s16 g_formation_ability_learn_selected_job_index;
extern menu_entry_t g_formation_ability_learn_confirm_menu[2];
/* The prompt chained after g_formation_ability_learn_confirm_menu. */
extern menu_entry_t g_formation_ability_learn_confirm_submenu;
/* Nonzero while the learn-confirmation menu thread (thread 6) is running. */
extern u8 g_formation_ability_learn_confirm_prompt_state;
extern s8 g_formation_ability_view_initialized;
extern s8 g_formation_ability_view_step; /* menu_job_list_step_e */
extern u8 g_formation_ability_view_confirm_prompt_state;
extern s16 g_formation_ability_view_job_names[];
void formation_job_wheel_menu(void);
s32 job_list_build(u8 unit_index, s16* jobs, s32 mode);
s32 formation_job_is_locked_for_all(s16 job_id);
s32 job_list_get_base(s16 unit_id);
s32 job_list_calculate_level(s32 index);
s32 job_list_calculate_total_jp(s32 index);
s32 formation_ability_get_chosen_job_level(s32 unused_row);
s32 formation_ability_get_chosen_job_next_jp(s32 unused_row);
s32 formation_ability_is_chosen_job_mastered(s32 unused_row);
s32 formation_ability_get_chosen_job_total_jp(s32 unused_row);
s32 formation_ability_get_chosen_job_jp(s32 unused_row);

/* abilities */
extern s16 g_formation_ability_category; /* menu_ability_category_e */
extern menu_entry_t g_formation_ability_menu;
extern s8 g_formation_ability_menu_initialized;
extern u8 g_formation_ability_menu_unit_index;
extern u8 g_formation_ability_menu_slots_changed;
extern u8 g_formation_ability_menu_saved_browse_enabled;
extern u8 g_formation_ability_submenu_running;
extern RECT g_formation_ability_panel_origin;
extern menu_status_thread_t g_formation_ability_panel_thread_params;
extern menu_instruction_t g_formation_ability_list_script;
extern menu_list_page_t g_formation_ability_list_pages[][2];
extern s8 g_formation_ability_view_list_initialized;
extern s8 g_formation_ability_set_initialized;
extern u8 g_formation_ability_set_unit_index;
extern menu_instruction_t g_formation_ability_set_window_script;
extern s8 g_formation_ability_set_slot; /* ability slot under the cursor: 0..4 */
extern u8 g_formation_ability_set_cursor;
extern u16 g_formation_ability_set_cursor_anim;
extern point16_t g_formation_ability_set_cursor_point;
extern s8 g_formation_ability_set_list_open;          /* ability list open */
extern s8 g_formation_ability_set_entry_count;        /* ability list entry count */
extern s8 g_formation_ability_set_list_close_pending; /* close the ability list this frame */
extern s8 g_formation_ability_set_show_stat_preview;  /* show the stat preview window */
extern s8 g_formation_ability_set_unread_flag;        /* cleared when Set Ability opens; never read */
extern s8 g_formation_ability_remove_initialized;
extern s8 g_formation_ability_remove_unit_index;
extern s8 g_formation_ability_remove_cursor;
extern u16 g_formation_ability_remove_cursor_anim;
extern point16_t g_formation_ability_remove_cursor_point;
extern s8 g_formation_ability_remove_all_prompt_active;
extern volatile u16 g_formation_ability_remove_all_blink_counter;
extern point16_t g_formation_ability_remove_all_cursor_point;
void formation_ability_learn(s16 unit_id, s16 job_id, s16 ability_id);
s32 formation_ability_learn_list(void);
s32 formation_ability_build_rsm_list(s16 unit_index, s32 slot, s16* out);
void formation_ability_diff_movement_stats(
    menu_status_panel_slot_storage_t* stat_diff, s32 old_ability, s32 new_ability);
s32 formation_ability_has_secret_hunt(s32 unit_id);
s32 formation_ability_has_two_hands(s16 unit_id);
s32 ability_has_two_swords(s16 unit_id);

/* equipment and items */
extern menu_status_thread_t g_formation_equipment_panel_thread_params;
extern s8 g_formation_item_equip_menu_initialized;
extern u16 g_formation_item_equip_item_cursor_anim;
extern point16_t g_formation_item_equip_item_cursor_point;
extern point16_t g_formation_item_remove_all_cursor_point;
extern volatile u16 g_formation_item_remove_all_blink_counter;
extern s8 g_formation_item_remove_unit_index;
extern u16 g_formation_item_remove_cursor_anim;
extern point16_t g_formation_item_remove_cursor_point;
extern s16 g_formation_item_remove_saved_primary_input;
extern s16 g_formation_item_remove_saved_secondary_input;
extern u16 g_formation_item_remove_saved_newly_pressed;
void formation_item_menu(void);

/* monster breeding */
typedef enum breeding_rarity {
    BREEDING_RARITY_NONE = 0,
    BREEDING_RARITY_COMMON = 1,
    BREEDING_RARITY_UNCOMMON = 2,
    BREEDING_RARITY_RARE = 3,
} breeding_rarity_e;

enum { BREEDING_RARITY_COUNT = 3 };

/* One inclusive date limit and its zodiac ID. The 13-entry WORLD table has
 * Capricorn at both calendar ends and does not contain Serpentarius. */
typedef struct zodiac_date_limit {
    calendar_date_t date;
    u8 zodiac;
} zodiac_date_limit_t;
typedef char world_zodiac_date_limit_size_must_be_3[(sizeof(zodiac_date_limit_t) == 3) ? 1 : -1];

/* Table sizes; the breeding table has one entry per monster job. */
enum {
    ZODIAC_DATE_LIMIT_COUNT = 13,
    BREEDING_MONTH_COUNT = 13,
    BREEDING_ENTRY_COUNT = JOB_ID_TIAMAT - JOB_ID_CHOCOBO + 1,
};

/* Provisional: the breeding entry of a monster job (JOB_ID_CHOCOBO on): the hatch-day range and the three offspring
 * jobs by rarity class (breeding_update). */
typedef struct breeding_entry {
    u8 hatch_range;                                    /* 0x00: rand() % hatch_range + 1 (at least 2) egg days */
    u8 offspring_job_by_rarity[BREEDING_RARITY_COUNT]; /* 0x01; common through rare */
} breeding_entry_t;
typedef char world_unit_monster_breed_entry_size_must_be_4[(sizeof(breeding_entry_t) == 4) ? 1 : -1];

/* Provisional: month lengths indexed by 1-based month, read by breeding_update. */
extern const u8 g_breeding_month_lengths[BREEDING_MONTH_COUNT];
extern const zodiac_date_limit_t g_zodiac_date_limits[ZODIAC_DATE_LIMIT_COUNT];
extern const breeding_entry_t g_breeding_table[BREEDING_ENTRY_COUNT];
void breeding_update(s32 unused);

#endif
