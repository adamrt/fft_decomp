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

/* world_unit_t::proposition_status, copied from the party record:
 * the label the formation grid draws below a unit that is away. */
typedef enum world_formation_unit_proposition {
    WORLD_FORMATION_UNIT_PROPOSITION_LEAVING = 1,
    WORLD_FORMATION_UNIT_PROPOSITION_MISSING = 2,
} world_formation_unit_proposition_e;

enum {
    WORLD_FORMATION_PREVIEW_UNIT_INDEX = PARTY_ROSTER_SLOT_COUNT,
    WORLD_FORMATION_UNIT_RECORD_COUNT = WORLD_FORMATION_PREVIEW_UNIT_INDEX + 1,
};

/* The WORLD menus' record of a unit (g_world_unit_records), compacted after roster and menu-specific filtering;
 * roster_slot keeps its roster identity. The builder and sorter produce the ordered g_world_unit_pointers list, not
 * a roster-indexed array. The byte fields from 0x70 on mirror the party_unit_t layout. Magical shield and accessory
 * evade are provisionally placed at 0x50/0x52, after the physical fields, to follow the 16-bit sequence. */
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
    u8 equippable_item_types[4]; /* 0x073; bit per item type, read by world_unit_can_equip_item_id */
    u8 unlocked_jobs[UNIT_UNLOCKED_JOB_BYTE_COUNT];                                           /* 0x077 */
    u8 learned_abilities[UNIT_LEARNED_ABILITY_JOB_COUNT][UNIT_LEARNED_ABILITY_BYTES_PER_JOB]; /* 0x07a through 0x0b2 */
    u8 job_levels[UNIT_JOB_LEVEL_BYTE_COUNT];                                                 /* 0x0b3 */
    u8 _padding_0bd;                                                                          /* aligns job_points */
    unit_job_points_t job_points; /* 0x0be; current JP followed by total JP */
    u8 name[UNIT_NAME_SIZE];      /* 0x10e; "unit Nickname" */
    u8 monster_graphic;           /* 0x11e; added to formation graphic entry 0x3d when sprite_set == 0x82
                                     (world_formation_build_unit_graphic_entry) */
    u8 proposition_status;        /* 0x11f; "Proposition Byte": world_formation_unit_proposition_e, 0 when present */
    u16 birthday;                 /* 0x120; unit_stats_t birthday & 0x1ff; egg readers split
                                     it into nibbles */
    u16 egg_color;           /* 0x122; party_unit_t 0xd2; world_formation_draw_unit_slots loads it as a 16-bit palette
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

/* Command texts of the formation's Item and Ability menus, chosen by the
 * selected unit's world_formation_unit_lock_e. */
enum {
    WORLD_FORMATION_ITEM_MENU_TEXT_UNLOCKED = 0xd801,
    WORLD_FORMATION_ITEM_MENU_TEXT_LOCKED = 0xd802,
    WORLD_FORMATION_ABILITY_MENU_TEXT_UNLOCKED = 0xd80b,
    WORLD_FORMATION_ABILITY_MENU_TEXT_LOCKED = 0xd80c,
    WORLD_FORMATION_ABILITY_MENU_TEXT_MIME = 0xd80d,
};

extern struct world_unit g_world_unit_records[WORLD_FORMATION_UNIT_RECORD_COUNT];
extern world_unit_t* g_world_unit_pointers[];
extern s16 g_world_formation_record_count; /* records in g_world_unit_records */
extern s16 g_world_formation_record_order[];
/* Menu callers cache the builder's return here; rebuilding the record list
 * does not itself refresh this byte. */
extern u8 g_world_formation_unit_count;
extern struct world_unit* g_world_formation_temp_unit;
s16 world_formation_build_record_list(s32 item_id, world_unit_t** unused_list, s32 mode);
void world_formation_build_unit_record(
    const struct unit_stats* unit, struct world_unit* record, const struct party_unit* party);
void world_formation_rebuild_unit_list(void);
s32 world_formation_find_index_by_roster_slot(s16 roster_slot);
void world_formation_copy_unit_to_temp(s16 index, s32 unused_slot);
void world_formation_save_records_to_party(void);
void world_formation_sort_roster(s32 index);
s32 world_formation_get_unit_lock(s32 unit_id);
s32 world_formation_generate_unit(s16 unit_type);
void world_formation_set_unit_name(s16 formation_index, const u8* name);
s32 world_formation_change_unit_job(s16 unit_id, s16 job_id, s32 update_equipment);
s32 world_formation_set_unit_ability_slot(s16 unit_id, s16 slot, s16 ability, s32 update_equipment);
s32 world_formation_can_equip_item_in_slot(s16 unit_index, s16 slot, s16 item_id);
s32 world_formation_equip_item_to_unit_slot(s16 unit, s16 slot, s16 item_id);
void world_formation_remove_invalid_unit_loadout(s16 unit_id);
void world_formation_recalculate_selected_unit_stats(void);

/* formation screen */
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

/* Screen x and texture u of one background tile; two alternating rows of four. */
typedef struct world_formation_background_tile {
    u16 x;
    u16 u;
} world_formation_background_tile_t;
typedef char world_formation_background_tile_size_must_be_4[(sizeof(world_formation_background_tile_t) == 4) ? 1 : -1];

extern s32 g_world_formation_current_menu; /* world_formation_menu_e */
extern void (*g_world_formation_menu_handlers[])(void);
extern s16 g_world_formation_open_request_state; /* world_screen_request_state_e */
extern u8 g_world_formation_screen_running;
extern s8 g_world_formation_main_menu_initialized;
extern menu_input_state_t g_world_menu_input_state;
/* Alias for the first 32-bit field retained for the target's separate absolute store. */
extern s32 g_world_menu_new_buttons;
extern u32 g_world_menu_allowed_buttons; /* pad mask applied to the new presses of each menu frame */
extern u32* g_world_formation_ordering_table;
/* g_main_system_flags as saved by world_menu_hide_world_map (main_system_flags_e). */
extern POLY_FT4 g_world_formation_view_quads[2][30];
extern world_formation_background_tile_t g_world_formation_background_tiles[2][4];
extern u8 g_world_formation_menu_text_table[];
extern u8 g_world_formation_menu_text_buffer[];
extern clut_t g_world_formation_menu_palettes;
extern menu_entry_t g_world_formation_triangle_menu;
extern u8 g_world_formation_triangle_menu_open;
extern u8 g_world_formation_triangle_menu_running;
extern menu_entry_t g_world_formation_egg_triangle_menu;
/* Chained after the egg triangle menu; world_formation_process_frame only resets it. */
extern menu_entry_t g_world_formation_egg_submenu;
void world_formation_run_screen(s32 tutorial, s32 unused);
void world_formation_run_screen_for_menu_index(s32 menu_index);
s16 world_menu_run_frame(u32* otag, u32 input);
void world_formation_init_menu_state(void);
void world_formation_init_menu_display(void);
void world_menu_reset_context(void);
void world_formation_restore_menu_after_core_exit(void);
void world_formation_stop_menu_threads(void);
void world_menu_set_allowed_buttons(u32 mask);
void world_formation_set_ordering_table_pointer(u32* ordering_table);
void world_formation_set_display_mode(s32 mode);
void world_formation_draw_background_tiles(s32 y, s32 unused_1);
void world_formation_view_zoom_thread(void);

/* unit grid */
extern oriented_quad_t g_world_formation_cursor_sprite;
extern oriented_quad_t g_world_formation_cursor_shadow_sprite;
extern point16_t g_world_formation_cursor_position;
extern point16_t g_world_formation_cursor_target;
/* Cursor trail position history, reset by world_formation_reset_slots_and_frame_tpages; entry 7 is the newest and
 * x is -1 in an unused entry. */
extern point16_t g_world_formation_cursor_trail[8];
extern u8 g_world_formation_cursor_trail_brightness[8];
extern u8 g_world_formation_cursor_ot_override;
extern s8 g_world_formation_selection_cursor_ready;
extern s8 g_world_formation_scroll_position;
extern s8 g_world_formation_scroll_velocity;
/* Cleared with the formation scroll velocity; nothing reads it. */
extern s8 g_world_formation_scroll_unread; /* cleared with the formation scroll velocity; never read */
extern u8 g_world_formation_unit_cycle_mode;
extern u8 g_world_formation_unit_browse_enabled;
extern s16 g_world_formation_selected_unit_index_latch;
extern oriented_quad_t g_world_formation_unit_dot_sprite;
extern s8 g_world_formation_dot_glow_timer;
extern s8 g_world_formation_dot_glow_direction;
/* Three consecutive 20-byte sprite records. */
extern oriented_quad_t g_world_formation_shadow_egg_sprites[3];
extern rgb24_t g_world_formation_away_unit_color;
extern rgb24_t g_world_formation_saved_sprite_color;
extern rgb24_t g_world_formation_dimmed_sprite_color;
extern rgb24_t g_world_formation_normal_sprite_color;
extern oriented_quad_t g_world_formation_label_cant_equip;
extern oriented_quad_t g_world_formation_label_equipping;
extern oriented_quad_t g_world_formation_label_leaving;
extern oriented_quad_t g_world_formation_label_missing;
extern menu_sprite_cell_t g_world_formation_label_tiles[];
extern menu_sprite_cell_t g_world_formation_label_tiles_bottom[];
extern u8 g_world_formation_grid_transition_frame;
extern u8 g_world_formation_grid_return_frame;
extern u8 g_world_formation_grid_return_done;
/* get_mode and get_shade take the unit slot index; NULL selects the default. */
s32 world_formation_update_and_draw_unit_grid(s16 background_y, s16 toggle_banner, s16 allow_browse, s16 input,
    s16 locked, s32 (*get_mode)(s32), s32 (*get_shade)(s32));
void world_formation_update_unit_selection(void);
void world_formation_cycle_selected_unit(s16 pad_buttons);
void world_formation_init_cursor_and_scroll_state(s32 formation_index);
void world_formation_init_selection_cursor(s16 formation_index);
s32 world_formation_is_unit_list_scroll_needed(void);
s32 world_formation_step_grid_transition(s32 mode);
s32 world_formation_step_grid_return_transition(s32 mode);

/* unit sprites and portraits */
extern texture_source_t g_world_formation_graphic_entries[];
extern u8 g_world_formation_generic_human_graphic_index_table[];
/* Portrait code per sprite set: low byte is the portrait index. */
extern u8 g_world_formation_unit_slide_pending;
extern volatile u8 g_world_formation_unit_slide_frame;
extern u8 g_world_formation_portrait_slide_frame;
s32 world_get_party_unit_formation_sprite(s32 party_index);
s32 world_formation_get_unit_palette_index(s32 roster_slot);
void world_formation_build_unit_graphic_entry(s16 formation_index, texture_source_t* destination, s32 unused);
void world_formation_get_generic_human_graphic_entry(s32 job_id, s16 is_female, texture_source_t* destination);
void world_formation_draw_unit_sprite(s16 unit_index, point16_t* pos, rgb24_t* rgb, s32 ot_index);
void world_formation_draw_sprite_with_shadow(const oriented_quad_t* pos, const rgb24_t* rgb, s32 ot_index);
void world_formation_place_unit_portrait_at_rest(s16 unit_index);
u8 world_formation_step_unit_portrait_slide(void);
void world_formation_stage_selected_unit(void);

/* unit banner and stat bar */
/* Set while the selected unit is in the upper rows of the grid: the unit banner
 * then sits at the bottom of the screen and the stat bar at the top (BUNIT's
 * g_bunit_status_banner_at_bottom). The latch is the value the banner was last
 * placed for. Majority spelling: five readers sign-extend as u8. Two files
 * (world_formation_init_selection_cursor, world_formation_step_grid_transition)
 * need s8 to match and keep their own local extern. */
extern u8 g_world_formation_unit_banner_at_bottom;
extern u8 g_world_formation_unit_banner_at_bottom_latch;
extern u8 g_world_formation_unit_banner_enabled;
extern u8 g_world_formation_saved_banner_enabled;
extern s8 g_world_formation_banner_slide_step;
extern u8 g_world_formation_stat_display_mode; /* menu_unit_readout_e, HP to BRAVE_FAITH */
extern menu_instruction_t g_world_formation_stat_bar_window_top;
extern menu_instruction_t g_world_formation_stat_bar_window_bottom;
extern menu_draw_number_command_t g_world_formation_gauge_value_number;
extern menu_draw_number_command_t g_world_formation_gauge_max_number;
extern menu_draw_number_command_t g_world_formation_level_number;
extern menu_draw_number_command_t g_world_formation_experience_number;
extern menu_draw_number_command_t g_world_formation_brave_number;
extern menu_draw_number_command_t g_world_formation_faith_number;
extern menu_draw_number_command_t g_world_formation_brave_number_wide;
extern menu_draw_number_command_t g_world_formation_faith_number_wide;
extern s8 g_world_formation_gauge_bar_offsets[4];
extern rgb24_t g_world_formation_hp_gauge_colors[4];
extern rgb24_t g_world_formation_mp_gauge_colors[4];
extern rgb24_t g_world_formation_ct_gauge_colors[4];

/* unit panels */
extern CVECTOR g_world_unit_panel_bar_colors[];
extern image_load_parameters_t g_world_unit_summary_label_sprite_params[];
extern image_load_parameters_t g_world_unit_summary_value_sprite_params[];
extern RECT g_world_unit_status_panel_origin;
extern image_load_parameters_t g_world_unit_status_sprite_params[];
extern image_load_parameters_t g_world_unit_status_portrait_params;
extern u8 g_world_unit_status_banner_active;
extern menu_entry_t g_world_unit_status_banner_menu;
extern s8 g_world_unit_status_banner_state; /* banner state: 0 idle, 1 finishing, else countdown */
s32 world_unit_get_facing_sixteenth_by_runtime_id(s32 runtime_id);

/* dismissal */
/* An entry of the sprite-set to script-variable table the dismissal tail walks (g_world_formation_dismiss_entries),
 * terminated by sprite_set == -1. The four-byte stride is proved by the target advancing both its record pointer and
 * its byte-offset index by 4 per iteration. */
typedef struct world_formation_dismiss_entry {
    s16 sprite_set;  /* 0x00 */
    s16 variable_id; /* 0x02 */
} world_formation_dismiss_entry_t;

extern world_formation_dismiss_entry_t g_world_formation_dismiss_entries[];
extern menu_entry_t g_world_formation_dismiss_prompt;
extern s16 g_world_formation_dismiss_prompt_text_id;
extern s16 g_world_formation_dismiss_prompt_kind;
extern menu_entry_t g_world_formation_dismiss_confirm_menu[4];
extern menu_entry_t g_world_formation_dismiss_confirm_menu_scrolled[4];
extern menu_entry_t* g_world_formation_dismiss_menu_data;
extern u8 g_world_formation_dismiss_step;
extern s16 g_world_formation_dismiss_anim_timer;
extern s8 g_world_dismiss_unit_prompt_running;
s32 world_formation_can_dismiss_unit(s16 formation_idx);
void world_formation_dismiss_unit(s16 formation_idx);
void world_formation_run_dismiss_unit_menu_step(void);

/* order and best fit */
extern menu_entry_t g_world_order_unit_menu;
extern u8 g_world_order_unit_saved_browse_enabled;
extern menu_entry_t g_world_best_fit_confirm_menu;
void world_unit_build_best_fit_equipment(s16 unit_index, s16* equipment);
s32 world_unit_can_equip_item_id(s16 unit_index, s16 item_id);

/* job wheel and learning */
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

extern s8 g_world_job_wheel_initialized;
extern s16 g_world_job_wheel_unit_index;
extern s16 g_world_job_wheel_jobs[];
extern s16 g_world_job_wheel_mastered_jobs[];
extern s16 g_world_job_wheel_job_count;
extern s16 g_world_job_wheel_cursor_index;
extern u8 g_world_job_wheel_job_selected;
extern u8 g_world_job_wheel_saved_browse_enabled;
extern menu_instruction_t g_world_job_wheel_script;
extern s16 g_world_job_wheel_name_text;
extern RECT g_world_job_wheel_name_rect;
/* The current and the new job's portrait; separate objects, since
 * world_formation_run_job_wheel_menu addresses each with its own lui/addiu. */
extern textured_rect_source_t g_world_job_wheel_portrait_quad;
extern textured_rect_source_t g_world_job_wheel_new_portrait_quad;
extern oriented_quad_t g_world_job_wheel_mastered_icon_sprite;
extern point16_t g_world_job_wheel_unit_sprite_position;
extern u8 g_world_job_wheel_center_x;
extern u8 g_world_job_wheel_center_y;
extern u8 g_world_job_wheel_animation_counter;
extern s8 g_world_job_wheel_spin_progress;
extern s8 g_world_job_wheel_spin_direction;
extern u8 g_world_job_wheel_opening;
extern u8 g_world_job_wheel_open_frame;
extern u8 g_world_job_wheel_closing;
extern u8 g_world_job_wheel_close_frame;
extern u8 g_world_job_change_active;
extern s8 g_world_job_change_anim_state; /* world_job_change_anim_e */
extern u8 g_world_job_change_confirmed;
extern s16 g_world_job_change_skip_requested;
extern s16 g_world_job_change_frame;
extern rgb24_t g_world_job_change_wave_colors[3];
extern rgb24_t g_world_job_change_flash_colors[4];
extern s16* g_world_job_change_dissolve_order;
extern RECT g_world_job_change_old_sprite_rect;
extern RECT g_world_job_change_new_sprite_rect;
extern u8* g_world_job_change_old_sprite_image;
extern u8* g_world_job_change_new_sprite_image;
extern u8* g_world_job_change_new_sprite_source;
extern s32 g_world_job_cached_total_jp;
extern s8 g_world_learn_menu_initialized;
extern s8 g_world_learn_menu_step; /* menu_job_list_step_e */
extern s8 g_world_learn_job_list_initialized;
extern menu_instruction_t g_world_learn_job_list_script;
extern s16 g_world_learn_job_name_list[];
extern s8 g_world_learn_ability_list_open;
extern s16 g_world_learn_selected_job;
extern s16 g_world_learn_selected_job_index;
extern menu_entry_t g_world_learn_confirm_menu[2];
/* The prompt chained after g_world_learn_confirm_menu. */
extern menu_entry_t g_world_learn_confirm_submenu;
/* Nonzero while the learn-confirmation menu thread (thread 6) is running. */
extern u8 g_world_learn_confirm_prompt_state;
extern s8 g_world_view_abilities_initialized;
extern s8 g_world_view_abilities_step; /* menu_job_list_step_e */
extern u8 g_world_view_abilities_confirm_prompt_state;
extern s16 g_world_view_job_name_list[];
void world_formation_run_job_wheel_menu(void);
s32 world_job_build_unit_list(u8 unit_index, s16* jobs, s32 mode);
s32 world_job_is_locked_for_all_units(s16 job_id);
s32 world_job_get_base(s16 unit_id);
s32 world_job_calculate_current_level(s32 index);
s32 world_job_calculate_current_total_jp(s32 index);
s32 world_job_get_cached_level(s32 unused_row);
s32 world_job_get_cached_level_jp_requirement(s32 unused_row);
s32 world_job_get_cached_skillset_empty_flag(s32 unused_row);
s32 world_job_get_cached_total_jp(s32 unused_row);
s32 world_job_get_selected_unit_job_points_entry(s32 unused_row);

/* abilities */
extern s16 g_world_ability_category; /* menu_ability_category_e */
extern menu_entry_t g_world_ability_menu;
extern s8 g_world_ability_menu_initialized;
extern u8 g_world_ability_menu_unit_index;
extern u8 g_world_ability_menu_slots_changed;
extern u8 g_world_ability_menu_saved_browse_enabled;
extern u8 g_world_ability_submenu_running;
extern RECT g_world_ability_panel_origin;
extern menu_status_thread_t g_world_ability_panel_thread_params;
extern menu_instruction_t g_world_ability_list_script;
extern menu_list_page_t g_world_ability_list_pages[][2];
extern s8 g_world_ability_view_list_initialized;
extern s8 g_world_set_ability_initialized;
extern u8 g_world_set_ability_unit_index;
extern menu_instruction_t g_world_set_ability_window_script;
extern s8 g_world_set_ability_slot; /* ability slot under the cursor: 0..4 */
extern u8 g_world_set_ability_cursor;
extern u16 g_world_set_ability_cursor_anim;
extern point16_t g_world_set_ability_cursor_point;
extern s8 g_world_set_ability_list_open;          /* ability list open */
extern s8 g_world_set_ability_entry_count;        /* ability list entry count */
extern s8 g_world_set_ability_list_close_pending; /* close the ability list this frame */
extern s8 g_world_set_ability_show_stat_preview;  /* show the stat preview window */
extern s8 g_world_set_ability_unread_flag;        /* cleared when Set Ability opens; never read */
extern s8 g_world_remove_ability_initialized;
extern s8 g_world_remove_ability_unit_index;
extern s8 g_world_remove_ability_cursor;
extern u16 g_world_remove_ability_cursor_anim;
extern point16_t g_world_remove_ability_cursor_point;
extern s8 g_world_clear_abilities_prompt_active;
extern volatile u16 g_world_clear_abilities_blink_counter;
extern point16_t g_world_clear_abilities_cursor_point;
void world_ability_learn(s16 unit_id, s16 job_id, s16 ability_id);
s32 world_ability_run_learn_list(void);
s32 world_ability_build_equippable_rsm_list(s16 unit_index, s32 slot, s16* out);
void world_ability_find_movement_stat_changes(
    menu_status_panel_slot_storage_t* stat_diff, s32 old_ability, s32 new_ability);
s32 world_ability_has_secret_hunt(s32 unit_id);
s32 world_ability_has_two_hands(s16 unit_id);
s32 world_ability_has_two_swords(s16 unit_id);

/* equipment and items */
extern menu_status_thread_t g_world_equipment_ability_panel_thread_params;
extern s8 g_world_equip_menu_initialized;
extern u16 g_world_equip_item_cursor_anim;
extern point16_t g_world_equip_item_cursor_point;
extern point16_t g_world_remove_all_equipment_cursor_point;
extern volatile u16 g_world_remove_all_equipment_blink_counter;
extern s8 g_world_remove_item_unit_index;
extern u16 g_world_remove_item_cursor_anim;
extern point16_t g_world_remove_item_cursor_point;
extern s16 g_world_remove_item_saved_primary_input;
extern s16 g_world_remove_item_saved_secondary_input;
extern u16 g_world_remove_item_saved_newly_pressed;
void world_formation_item_menu(void);

/* monster breeding */
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

/* Table sizes; the breeding table has one entry per monster job. */
enum {
    WORLD_ZODIAC_DATE_LIMIT_COUNT = 13,
    WORLD_MONTH_LENGTH_TABLE_COUNT = 13,
    WORLD_MONSTER_BREED_ENTRY_COUNT = JOB_ID_TIAMAT - JOB_ID_CHOCOBO + 1,
};

/* Provisional: the breeding entry of a monster job (JOB_ID_CHOCOBO on): the hatch-day range and the three offspring
 * jobs by rarity class (world_unit_update_monster_breeding). */
typedef struct world_unit_monster_breed_entry {
    u8 hatch_range; /* 0x00: rand() % hatch_range + 1 (at least 2) egg days */
    u8 offspring_job_by_rarity[WORLD_MONSTER_BREED_RARITY_COUNT]; /* 0x01; common through rare */
} world_unit_monster_breed_entry_t;
typedef char world_unit_monster_breed_entry_size_must_be_4[(sizeof(world_unit_monster_breed_entry_t) == 4) ? 1 : -1];

/* Provisional: month lengths indexed by 1-based month, read by world_unit_update_monster_breeding. */
extern const u8 g_world_month_lengths[WORLD_MONTH_LENGTH_TABLE_COUNT];
extern const world_zodiac_date_limit_t g_world_zodiac_date_limits[WORLD_ZODIAC_DATE_LIMIT_COUNT];
extern const world_unit_monster_breed_entry_t g_world_unit_monster_breed_table[WORLD_MONSTER_BREED_ENTRY_COUNT];
void world_unit_update_monster_breeding(s32 unused);

#endif
