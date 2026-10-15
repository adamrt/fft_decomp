/* WORLD/WORLD.BIN: world map, menus and event scripts. */
#ifndef WORLD_H
#define WORLD_H

#include "fft/bits.h"
#include "fft/camera.h"
#include "fft/card.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/main.h"
#include "fft/sound.h"
#include "fft/status.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/libgs.h"

/* dead unit */

extern RECT g_world_screen_menu_thin_panel_texture_windows[7];

/* One libgpu texture window per DR_MODE packet (the seven RECTs after
 * g_world_screen_menu_thin_panel_texture_windows). The static table holds each
 * window's w/h; world_screen_menu_window_frame_thread fills in the runtime x/y.
 * Indexing the array reproduces the target's symbol-plus-byte-offset addressing. */
extern RECT g_world_screen_menu_panel_texture_windows[7];
extern s16 g_world_dead_unit_menu_flags[512];
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

extern u16 g_world_menu_location_list_text_ids[];
extern s16 g_world_menu_location_list_row_colors[];
extern menu_scroll_text_layout_t g_world_menu_location_list_layout;
extern u8 g_world_location_names_resident[];

void world_menu_build_location_list(void);
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

extern u8 g_world_name_entry_character_rows[];
extern menu_instruction_t g_world_name_entry_display_script;
extern menu_instruction_t g_world_name_entry_keyboard_script;
extern s8 g_world_name_entry_started; /* name entry started */
extern u16 g_world_name_keyboard_text_cursor_anim;
extern u16 g_world_name_entry_text_cursor_anim;
extern RECT g_world_name_entry_screen_image_rect;
extern s16 g_world_name_entry_key_codes[];
extern u8 g_world_name_entry_confirm_message_shown;
extern u16 g_world_name_entry_key_cursor_anim;
extern u8 g_world_name_entry_char_to_special_column[];
extern u8 g_world_name_entry_special_to_char_column[];
extern u8 g_world_name_entry_cursor_x_positions[];
extern RECT g_world_name_entry_text_vram_rect;
extern s16 g_world_name_entry_text_row_ids[];
extern menu_entry_t g_world_name_confirm_menu;
extern u8 g_world_name_confirm_initialized;
extern s8 g_world_name_keyboard_last_state; /* last drawn state */
extern s8 g_world_name_entry_last_state;    /* last drawn name-entry state */
extern u8 g_world_name_entry_input_delay;
extern u8 g_world_name_confirm_finished;
extern u8 g_world_name_used_original_name;
extern s16 g_world_name_entry_key_ids[];
extern void (*g_world_name_entry_state_handlers[])(void);
extern u8 g_world_name_entry_state;
extern u8 g_world_name_entry_saved_state;
extern u8 g_world_name_entry_cursor_index;
extern u8 g_world_name_entry_list_cursor;
extern u8 g_world_name_entry_cursor;
extern u8 g_world_name_entry_text[17];
extern const u8* g_world_name_entry_original_name;
extern s16 g_world_name_confirm_result;
void world_name_init_entry(const u8* name);
void world_name_apply_entry_key(s16 key);
void world_name_run_confirmation_menu(void);
u8* world_name_run_entry_screen(const u8* list, u32* image);
u8* world_name_run_entry_keyboard_frame(const u8* list, s32 unused_1);
void world_name_update_entry_cursor(void);

/* options */

/* screen */

extern RECT g_world_screen_clear_rect;
extern fade_overlay_t g_world_screen_tint_tiles[]; /* one tint tile per packet buffer */

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

extern u32 g_world_event_current_input;
extern u32 g_world_event_initial_repeat_counter;
extern u32 g_world_event_previous_input;
extern u32 g_world_event_repeat_counter;
extern u32 g_world_event_secondary_repeat_counter;
extern u32 g_world_script_tutorial_button_input;
extern s32 g_world_script_tutorial_command_active;
extern u8 g_world_script_tutorial_id;
extern u16 g_world_script_tutorial_shift_command_bit;
extern s16 g_world_script_tutorial_wait_time_remaining;
extern s16 g_world_script_tutorial_wait_timer;
extern u16 g_world_script_tutorial_latched_button;
extern u8 g_world_script_tutorial_activation_timer;
extern s16 g_world_script_tutorial_buttons;
extern s32 g_world_script_saved_event_input;
extern u32 g_world_script_tutorial_repeat_counters[16];
extern u16 g_world_script_tutorial_previous_input;
extern u8* g_world_script_tutorial_command_ptr;
extern s16 g_world_event_effect_target_runtime_id;
extern world_event_command_args_t g_world_event_map_command_80_args;
extern world_event_command_args_t g_world_event_map_command_83_args;
extern world_event_music_switch_t g_world_event_music_switch;
void world_script_handle_tutorial_command_highlight_ring(void);
void world_script_advance_tutorial_highlight_brightness(void);
void script_earthquake_start(void);

s32 world_script_handle_tutorial_command(void);
void world_script_handle_tutorial_command_change_dialog(void);
void world_script_handle_tutorial_command_display_message(void);
s32 world_script_handle_tutorial_command_end(void);
void world_script_handle_tutorial_command_shift(void);
void world_script_handle_tutorial_command_wait(void);
void world_script_handle_tutorial_command_wait_for_button(void);
void world_script_handle_tutorial_command_wait_for_message(void);
void world_script_handle_tutorial_command_wait_time(void);
void world_script_start_tutorial(s32 index);
void world_script_update_tutorial_controller_input(void);

/* sound */
extern s16 g_world_sound_current_music_track_index;
extern s16 g_world_sound_music_switch_request;
extern s16 g_world_sound_music_unload_slot_request;
extern s32 g_world_sound_music_volume_transition_request;
extern u8 g_world_sound_release_held_loop;
void world_sound_dispatch_effect(void);

/* status */

/* Per-byte status removal masks applied while an event stages a unit. */

/* status panel */

/* system */

extern s32 g_menu_companion_executable_lbas[];    /* 0x80156388, indexed by request */
extern s32 g_menu_companion_executable_sizes[];   /* 0x801563c8 */
extern s32 g_menu_companion_executable_offsets[]; /* 0x80156408, relative to the overlay load address */
extern s32 g_menu_companion_executable_loading;   /* 0x801564cc */
void world_game_reset(void);
void world_card_close_stub(void);

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

/* world_build_at_list runs its window frame thread in the upper menu's slot. */
enum {
    WORLD_AT_LIST_THREAD_SLOT_WINDOW_FRAME = 15,
};

void world_text_release_current_message_box_slot(void);
void world_formation_ability_menu_idle_thread(void);
void world_thread_idle_yield_forever(void);
s32 world_thread_is_task_active(s32 unused_0);
void world_card_input_update_with_message_state(void);
void world_thread_yield_forever(void);
void world_thread_request_redraw(s32 thread_id);
void world_thread_request_stop(s32 thread_id);

/* turn */

void world_screen_build_at_list(void);

/* ability */
/* Final selector for the WORLD ability-list builder at 0x801228f0. */
typedef enum world_ability_list_mode {
    WORLD_ABILITY_LIST_MODE_VIEW_ALL = 0,
    WORLD_ABILITY_LIST_MODE_LEARNED_ONLY = 1,
    WORLD_ABILITY_LIST_MODE_LEARN_MENU = 2,
    WORLD_ABILITY_LIST_MODE_COUNT_UNLEARNED = 3,
} world_ability_list_mode_e;

extern s16 g_world_ability_category; /* menu_ability_category_e */
extern RECT g_world_ability_panel_origin;
extern RECT g_world_ability_panel_draw_area;
extern menu_status_thread_t g_world_ability_panel_thread_params;
extern s8 g_world_ability_menu_initialized;
extern s8 g_world_remove_ability_initialized;
extern s8 g_world_set_ability_initialized;
extern s8 g_world_ability_view_list_initialized;
extern u8 g_world_ability_menu_slots_changed;
extern menu_entry_t g_world_ability_menu;
extern u8 g_world_ability_submenu_running;
extern u8 g_world_ability_menu_unit_index;
extern s8 g_world_remove_ability_unit_index;
extern u16 g_world_remove_ability_cursor_anim;
extern point16_t g_world_remove_ability_cursor_point;
extern point16_t g_world_clear_abilities_cursor_point;
extern menu_instruction_t g_world_set_ability_window_script;
extern s8 g_world_set_ability_slot;               /* ability slot under the cursor: 0..4 */
extern s8 g_world_set_ability_list_close_pending; /* close the ability list this frame */
extern s8 g_world_set_ability_show_stat_preview;  /* show the stat preview window */
extern u16 g_world_set_ability_cursor_anim;
extern point16_t g_world_set_ability_cursor_point;
extern menu_instruction_t g_world_ability_list_script;
extern menu_list_page_t g_world_ability_list_pages[][2];
extern u8 g_world_ability_menu_saved_browse_enabled;
extern s8 g_world_remove_ability_cursor;
extern s8 g_world_clear_abilities_prompt_active;
extern s16 g_world_remove_ability_slots[5];
extern volatile u16 g_world_clear_abilities_blink_counter;
extern s8 g_world_set_ability_list_open;   /* ability list open */
extern s8 g_world_set_ability_entry_count; /* ability list entry count */
extern u8 g_world_set_ability_unit_index;
extern u8 g_world_set_ability_cursor;

void world_ability_find_stat_changes_due_to_equipped(
    menu_status_panel_slot_storage_t* stat_diff, s32 old_ability, s32 new_ability);

void world_ability_get_move_and_jump_increase_values(s16 ability, menu_status_panel_slot_storage_t* out);
s32 world_ability_has_secret_hunt(s32 unit_id);
s32 world_ability_has_two_hands(s16 unit_id);
s32 world_ability_has_two_swords(s16 unit_id);
s32 world_ability_is_category_action(s32 unused_row);
s32 world_ability_is_category_movement(s32 unused_row);
s32 world_ability_is_category_reaction(s32 unused_row);
s32 world_ability_is_category_support(s32 unused_row);
s32 world_ability_is_equip_support_change(s16 unit_id, s16 ability_id);
void world_ability_learn(s16 unit_id, s16 job_id, s16 ability_id);
s32 world_ability_run_view_list(void);
s32 world_ability_build_equippable_rsm_list(s16 unit_index, s32 slot, s16* out);
s32 world_ability_run_learn_list(void);

s32 world_ability_list_secondary_skillsets(s16 unit_id, s16* skillsets);

/* equipment */
typedef enum world_equipment_slot {
    WORLD_EQUIPMENT_SLOT_RIGHT_HAND = 0,
    WORLD_EQUIPMENT_SLOT_LEFT_HAND = 1,
    WORLD_EQUIPMENT_SLOT_HEAD = 2,
    WORLD_EQUIPMENT_SLOT_BODY = 3,
    WORLD_EQUIPMENT_SLOT_ACCESSORY = 4,
    WORLD_EQUIPMENT_SLOT_COUNT = 5,
} world_unit_equipment_slot_e;

extern RECT g_world_equipment_panel_origin;
extern RECT g_world_equipment_ability_panel_origin;
extern RECT g_world_equipment_panel_draw_area;
extern RECT g_world_equipment_ability_panel_draw_area;
extern RECT g_world_equipment_panel_item_text_rect;
extern RECT g_world_equipment_panel_ability_text_rect;
extern image_load_parameters_t g_world_equipment_panel_sprite_params_0[19];
extern image_load_parameters_t g_world_equipment_panel_sprite_params_1[19];
extern image_load_parameters_t g_world_equipment_panel_sprite_params_2[19];
extern image_load_parameters_t g_world_equipment_panel_sprite_params_3[19];
extern image_load_parameters_t g_world_equipment_panel_icon_params[WORLD_EQUIPMENT_SLOT_COUNT];
extern menu_status_thread_t g_world_equipment_ability_panel_thread_params;
extern s8 g_world_equip_menu_initialized;
extern u16 g_world_equip_item_cursor_anim;
extern point16_t g_world_equip_item_cursor_point;
extern u16 g_world_remove_all_equipment_ids[WORLD_EQUIPMENT_SLOT_COUNT];
extern point16_t g_world_remove_all_equipment_cursor_point;
extern volatile u16 g_world_remove_all_equipment_blink_counter;

/* editor */

/* comparison */
extern menu_unit_status_record_t g_world_comparison_unit_stat_summary;
extern menu_unit_status_identity_t g_world_comparison_unit_identity;
extern menu_number_entry_t g_world_comparison_unit_number_entries[];
extern RECT g_world_comparison_unit_portrait_rect;
extern menu_status_panel_editor_packet_t g_world_comparison_unit_summary_packets[2];
extern menu_status_panel_packet_t g_world_comparison_unit_status_packets[2];
extern u8 g_world_comparison_unit_stat_text_image[];
extern u8 g_world_comparison_unit_name_text_image[];
extern menu_unit_number_images_t g_world_comparison_unit_number_text_images;
extern palette_t g_world_comparison_unit_portrait_palettes[2];

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

extern menu_status_panel_buffer_t g_world_change_banner_panel_frames[2];

/* Scenario interpreter interfaces shared by the BATTLE and WORLD
 * script_execute_event_variant bodies. Signatures are as the interpreters bind them;
 * unverified names stay provisional. */
extern s16 g_world_3d_object_use_request;
extern u8 g_world_gfx_fade_decreasing;
extern u8 g_world_gfx_fade_disabled;
extern u8 g_world_gfx_fade_increasing;
extern s16 g_world_gfx_fade_intensity;
extern glyph_blit_t g_world_gfx_image_blit_source;
extern RECT g_world_gfx_temporary_draw_area;
extern u16 g_world_gfx_draw_area_scale_percent[];

/* A full-screen texture window (x=0, y=0, w=0x100, h=0x100 in the WORLD data
 * at 0x8016e44c), copied verbatim into a menu record's DR_MODE by
 * world_screen_gfx_init_scaled_draw_area_packets; the formation view reads the same
 * bytes as a zero screen point. Same 8-byte RECT shape as
 * g_gfx_texture_window, whose reset sibling is byte-for-byte identical. */
extern const RECT g_world_gfx_full_texture_window;

/* Frame-buffer rectangles for the four WLDFACE pages; four separate objects,
 * since as RECT[4] world_gfx_load_wldface_to_frame_buffer shares one base. */
extern RECT g_world_wldface_vram_rects;
extern RECT g_world_wldface_vram_rect_1;
extern RECT g_world_wldface_vram_rect_2;
extern RECT g_world_wldface_vram_rect_3;
extern u16 g_world_portrait_clut_by_sheet_id[]; /* 16-bit CLUT value per sprite-sheet id */
extern const u16 g_world_gfx_panel_draw_area_scale_percent[12];
extern rgb24_t g_world_gfx_black_tile_color;
extern menu_status_thread_t g_world_change_banner_panel_thread_params;
extern textured_quad_source_t g_world_vertical_cursor_quad;
extern RECT g_world_gfx_fade_tile_rect;
/* Sector and size pairs, read as flat 32-bit values ([i * 2], [i * 2 + 1]): a record
 * member access changes how the target addresses them. */
extern s32 g_world_shop_background_tim_files[9 * 2];
extern u16 g_world_gfx_draw_area_y;
extern u8 g_world_change_banner_text_image[];

/* Fade tile RGB channels, set to equal levels by the fade updaters. */
extern rgb24_t g_world_fade_tile_color;
extern u16 g_world_gfx_flat_line_capacity;     /* +0x24 line count */
extern u16 g_world_gfx_gradient_line_capacity; /* gradient line count */
extern u16 g_world_gfx_line_g3_capacity;
extern u16 g_world_gfx_flat_line_count;
extern u16 g_world_gfx_textured_quad_capacity; /* textured quad count */
extern u16 g_world_gfx_line_f3_count;
extern u16 g_world_gfx_gradient_line_count;
extern u16 g_world_gfx_textured_gradient_quad_capacity; /* textured gradient quad count */
extern u16 g_world_gfx_line_g3_count;
extern u16 g_world_gfx_line_f4_count;
extern u16 g_world_gfx_line_g4_count;
extern u16 g_world_gfx_poly_ft3_count;
extern u16 g_world_gfx_poly_gt3_count;
extern u16 g_world_gfx_draw_buffer_clip_y;

extern u16 g_world_ot_length;

/* Volatile: world_gfx_init_double_packet_buffers must read the draw-area count
 * after the draw-move count, as the target does. */
extern volatile u16 g_world_gfx_draw_area_capacity;
extern u16 g_world_gfx_draw_area_count;

/* Draw move count; volatile for the same read order as g_world_gfx_draw_area_capacity. */
extern volatile u16 g_world_gfx_draw_move_capacity;
extern u16 g_world_gfx_sprite_count;
extern u16 g_world_gfx_tile_1_count;
extern u16 g_world_gfx_tile_8_count;
extern menu_gfx_context_t* g_world_gfx_packet_buffers;
extern u16 g_world_gfx_sprite_8_count;

extern u16 g_world_gfx_tile_16_count;
extern u16 g_world_gfx_sprite_16_count;

extern u16 g_world_gfx_gradient_quad_capacity; /* gradient quad count */
extern u16 g_world_gfx_poly_f3_count;
extern u16 g_world_gfx_poly_g3_count;
extern u16 g_world_gfx_poly_f4_count;
extern s32 g_active_graphics_buffer_index;

void gfx_add_draw_area_primitive(RECT* rect, s32 otag_index);

void gfx_append_poly_f4_to_otag(point16_t* xy, rgb24_t* rgb, s32 semi_trans, s32 ot_index);

void world_gfx_draw_signed_number(s32 value, s32 digits, u8* image, glyph_blit_t* pen);

void world_gfx_flip_poly_ft4_u_if_negative(POLY_FT4* poly, s32 direction);

void world_gfx_init_double_packet_buffers(menu_gfx_context_t* buffers, u32* otags, void* unused_2, void* unused_3,
    void* unused_4, POLY_FT4* textured_quads, void* unused_6, void* unused_7, POLY_G4* gradient_quads,
    POLY_GT4* textured_gradient_quads, LINE_F2* flat_lines, void* unused_11, void* unused_12, LINE_G2* gradient_lines,
    void* unused_14, void* unused_15, TILE* tiles, void* unused_17, void* unused_18, void* unused_19, void* unused_20,
    void* unused_21, void* unused_22, DR_MOVE* draw_moves, DR_AREA* draw_areas, DR_MODE* draw_modes);

void world_gfx_init_packet_pools(menu_gfx_context_t* buffer);
void world_screen_menu_lerp_rect_to_poly_ft4(POLY_FT4* poly, const RECT* from, const RECT* to, s32 t);
void world_panel_gfx_init_scaled_draw_area_packets(scaled_draw_area_pair_t* pair);
void world_gfx_set_blit_dest_position(s32 x, s32 y);
void world_gfx_set_fade_disabled(u8 disabled);

s32 world_gfx_get_fade_state(void);
void world_gfx_load_wldface_to_frame_buffer(void);
void world_gfx_present_frame_and_swap_packet_buffer(s32 first_otag, s32 draw_otag);
void text_set_blit_destination(s16 x, s16 y);
void world_gfx_set_image_blit_source_rect(s16 x, s16 y, s16 width, s16 height);
void world_gfx_start_decreasing_fade(void);
void world_gfx_start_increasing_fade(void);
s32 world_gfx_update_fade_in_tile(void);
s32 world_gfx_update_fade_out_tile(void);

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

extern s32 g_world_job_cached_total_jp;
extern s16 g_world_job_wheel_job_count;
extern s16 g_world_job_wheel_name_text;
extern s8 g_world_learn_menu_initialized;
extern s8 g_world_job_wheel_initialized;
extern s8 g_world_learn_job_list_initialized;
extern s8 g_world_learn_ability_list_open;
extern s8 g_world_view_abilities_initialized;
extern s16 g_world_job_wheel_unit_index;

/* The current and the new job's portrait; separate objects, since
 * world_formation_run_job_wheel_menu addresses each with its own lui/addiu. */
extern textured_rect_source_t g_world_job_wheel_portrait_quad;
extern textured_rect_source_t g_world_job_wheel_new_portrait_quad;
extern menu_instruction_t g_world_job_wheel_script;
extern u8 g_world_job_wheel_job_selected;
extern point16_t g_world_job_wheel_unit_sprite_position;
extern u8 g_world_job_wheel_center_x;
extern u8 g_world_job_wheel_center_y;
extern u8 g_world_job_wheel_animation_counter;
extern RECT g_world_job_change_old_sprite_rect;
extern RECT g_world_job_change_new_sprite_rect;
extern u8 g_world_job_change_active;
extern s16 g_world_job_change_frame;
extern rgb24_t g_world_job_change_wave_colors[3];
extern u8 g_world_job_wheel_close_frame;
extern u8 g_world_job_wheel_open_frame;
extern u8 g_world_job_wheel_closing;
extern u8 g_world_job_wheel_opening;
extern oriented_quad_t g_world_job_wheel_mastered_icon_sprite;
extern menu_instruction_t g_world_learn_job_list_script;
extern menu_entry_t g_world_learn_confirm_menu[2];

/* The prompt chained after g_world_learn_confirm_menu. */
extern menu_entry_t g_world_learn_confirm_submenu;
extern s16 g_world_learn_job_name_list[];
extern s16 g_world_view_job_name_list[];
extern s8 g_world_job_wheel_spin_progress;
extern s8 g_world_job_wheel_spin_direction;
extern s8 g_world_job_change_anim_state; /* world_job_change_anim_e */
extern u8 g_world_job_change_confirmed;
extern u8 g_world_job_wheel_saved_browse_enabled;
extern rgb24_t g_world_job_change_flash_colors[4];
extern s16* g_world_job_change_dissolve_order;
extern u8* g_world_job_change_old_sprite_image;
extern u8* g_world_job_change_new_sprite_image;
extern u8* g_world_job_change_new_sprite_source;
extern s16 g_world_job_change_skip_requested;
extern s16 g_world_job_wheel_cursor_index;
extern s16 g_world_job_wheel_jobs[];
extern s16 g_world_job_wheel_mastered_jobs[];
extern RECT g_world_job_wheel_name_rect;
extern s8 g_world_learn_menu_step; /* menu_job_list_step_e */

/* Nonzero while the learn-confirmation menu thread (thread 6) is running. */
extern u8 g_world_learn_confirm_prompt_state;
extern s8 g_world_view_abilities_step; /* menu_job_list_step_e */
extern u8 g_world_view_abilities_confirm_prompt_state;
extern s16 g_world_learn_selected_job;
extern s16 g_world_learn_selected_job_index;
s32 world_job_build_unit_list(u8 unit_index, s16* jobs, s32 mode);

/* Job id -> generic job index 0..0x13, else 0. */
s32 world_job_get_text_index(s16 job_id);

s32 world_job_get_cached_level(s32 unused_row);
s32 world_job_get_cached_level_jp_requirement(s32 unused_row);
s32 world_job_get_cached_skillset_empty_flag(s32 unused_row);
s32 world_job_get_cached_total_jp(s32 unused_row);
s32 world_job_get_selected_unit_job_points_entry(s32 unused_row);
s32 job_is_locked_for_all_units(s16 job_id);

/* panel */

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

extern s16 g_world_tutorial_categories_request_state; /* world_screen_request_state_e */
extern world_tutorial_state_t g_world_tutorial_state;
/* Sector and size pairs; flat 32-bit value access preserves the original addressing. */
extern s32 g_world_script_tutorial_message_files[7 * 2];
extern s32 g_world_script_tutorial_script_files[7 * 2];
extern point16_t g_world_tutorial_highlight_corners_a[31][2]; /* outer/middle ring samples */
extern point16_t g_world_tutorial_highlight_corners_b[31][2]; /* middle/inner ring samples */
extern rgb24_t g_world_tutorial_highlight_colors_a[4];        /* first quad corner colors */
extern rgb24_t g_world_tutorial_highlight_colors_b[4];        /* second quad corner colors */
extern u8 g_world_tutorial_highlight_quad_count;
void world_script_draw_tutorial_highlight_quads(s32 brightness);
void world_script_load_tutorial_files(s32 index, world_tutorial_state_t* record);

/* input */
extern menu_input_state_t g_world_input_frame_state;
extern u32 g_world_input_frame_current;
extern u32 g_world_input_frame_initial_repeat_counter;
extern u32 g_world_input_frame_new;
extern u32 g_world_input_frame_previous;
extern u32 g_world_input_frame_repeat_counter;
extern u32 g_world_input_frame_secondary_repeat_counter;

/* Countdown maintained by world_menu_update_thread_7_idle_countdown. Volatile:
 * that routine reloads it after the zero test and again before the running
 * check instead of reusing the first load. */
extern volatile u8 g_world_input_lockout_timer;
extern u16 g_world_input_previous, g_world_input_latched_button;
extern u32 g_world_input_primary_repeat;
extern menu_input_timing_profile_t g_world_input_timing_profiles[];
extern u8 g_world_input_triangle_latched;
extern u8 g_world_input_triangle_latch;
extern u8 g_world_input_blocked_state;
extern s32 g_world_input_primary_repeat_latched;
extern s32 g_world_input_secondary_repeat_latched;
s32 world_input_filter_menu(void);
s32 world_input_get_lockout_timer(void);
s32 world_input_get_tutorial_buttons(void);
s32 world_input_read_controller(s32 unused);
s32 menu_input_read_tutorial_or_controller(s32 unused_0);
void world_input_update_controller(void);

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
    s16 magical_attack;                        /* 0x04c */
    s16 magical_class_evade;                   /* 0x04e */
    s16 magical_shield_evade;                  /* 0x050 */
    s16 magical_accessory_evade;               /* 0x052 */
    u16 equipment[WORLD_EQUIPMENT_SLOT_COUNT]; /* 0x054; right hand, left hand, head, body, accessory */
    world_formation_abilities_t abilities;     /* 0x05e */
    u8 _unused_068[8];                         /* 0x068; spare tail of the stat-detail block */
    u8 gender_flags;                           /* 0x070; unit_flags_e (fft/unit.h) */
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

extern oriented_quad_t g_world_formation_cursor_shadow_sprite;
extern world_formation_background_tile_t g_world_formation_background_tiles[2][4];
extern oriented_quad_t g_world_formation_cursor_sprite;
extern s32 g_world_formation_current_menu; /* world_formation_menu_e */
extern point16_t g_world_formation_cursor_position;
extern u8 g_world_formation_generic_human_graphic_index_table[];
extern texture_source_t g_world_formation_graphic_entries[];
extern menu_input_state_t g_world_formation_menu_input_state;

/* Alias for the first 32-bit field retained for the target's separate absolute store. */
extern s32 g_world_formation_menu_new_buttons;
extern u32* g_world_formation_ordering_table;
/* g_main_system_flags as saved by world_formation_save_and_set_system_flags (main_system_flags_e). */
extern u32 g_world_formation_saved_system_flags;

/* Set while the selected unit is in the upper rows of the grid: the unit banner
 * then sits at the bottom of the screen and the stat bar at the top (BUNIT's
 * g_bunit_status_banner_at_bottom). The latch is the value the banner was last
 * placed for. Majority spelling: five readers sign-extend as u8. Two files
 * (world_formation_init_selection_cursor, world_formation_step_grid_transition)
 * need s8 to match and keep their own local extern. */
extern u8 g_world_formation_unit_banner_at_bottom;
extern u8 g_world_formation_unit_banner_at_bottom_latch;
extern s16 g_world_formation_selected_unit_index_latch;
extern struct world_unit* g_world_formation_temp_unit;
extern u8 g_world_formation_unit_banner_enabled;
extern u8 g_world_formation_unit_browse_enabled;

/* Provisional: month lengths indexed by 1-based month (0x8018deac; follows
 * the zodiac date limits at 0x8018de84). Read by world_unit_update_monster_breeding. */
extern const u8 g_world_month_lengths[WORLD_MONTH_LENGTH_TABLE_COUNT];
extern const world_zodiac_date_limit_t g_world_zodiac_date_limits[WORLD_ZODIAC_DATE_LIMIT_COUNT];
extern void (*g_world_formation_menu_handlers[])(void);
extern s16 g_world_formation_open_request_state;   /* world_screen_request_state_e */
extern u32 g_world_formation_menu_allowed_buttons; /* pad mask applied to the new presses of each menu frame */
extern s32 g_world_formation_name_string_revision;
extern zodiac_draw_context_t g_world_formation_view;

/* Portrait code per sprite set: low byte is the portrait index. */
extern u16 g_world_formation_sprite_by_portrait[];

extern u8 g_world_formation_menu_text_table[];
extern clut_t g_world_formation_menu_palettes;
extern rgb24_t g_world_formation_saved_sprite_color;
extern rgb24_t g_world_formation_dimmed_sprite_color;
extern rgb24_t g_world_formation_normal_sprite_color;
extern u8 g_world_formation_unit_cycle_mode;
extern u8 g_world_formation_cursor_ot_override;
extern s8 g_world_formation_main_menu_initialized;
extern u8 g_world_formation_saved_banner_enabled;
extern u8 g_world_formation_screen_running;
extern u32 g_world_formation_status_label_image[];
extern RECT g_world_formation_status_label_image_rect;
extern menu_entry_t g_world_formation_egg_triangle_menu;

/* Chained after the egg triangle menu; world_formation_process_frame only resets it. */
extern menu_entry_t g_world_formation_egg_submenu;
extern u8 g_world_formation_unit_slide_pending;
extern s8 g_world_formation_scroll_position;
extern s8 g_world_formation_scroll_velocity;
extern s8 g_world_formation_selection_cursor_ready;
extern u8 g_world_formation_triangle_menu_open;
extern u8 g_world_formation_triangle_menu_running;
extern volatile u8 g_world_formation_unit_slide_frame;
extern s8 g_world_formation_banner_slide_step;
extern u8 g_world_formation_stat_display_mode; /* menu_unit_readout_e, HP to BRAVE_FAITH */
extern menu_draw_number_command_t g_world_formation_gauge_value_number;
extern menu_draw_number_command_t g_world_formation_level_number;
extern menu_draw_number_command_t g_world_formation_experience_number;
extern menu_draw_number_command_t g_world_formation_brave_number;
extern menu_draw_number_command_t g_world_formation_faith_number;
extern menu_draw_number_command_t g_world_formation_brave_number_wide;
extern menu_draw_number_command_t g_world_formation_faith_number_wide;
extern menu_draw_number_command_t g_world_formation_gauge_max_number;
extern menu_instruction_t g_world_formation_stat_bar_window_top;
extern menu_instruction_t g_world_formation_stat_bar_window_bottom;

/* Three consecutive sprite records at 0x8018c704 / 0x8018c718 / 0x8018c72c. */
extern oriented_quad_t g_world_formation_shadow_egg_sprites[3];
extern s8 g_world_formation_gauge_bar_offsets[4];
extern rgb24_t g_world_formation_hp_gauge_colors[4];
extern rgb24_t g_world_formation_mp_gauge_colors[4];
extern rgb24_t g_world_formation_ct_gauge_colors[4];
extern oriented_quad_t g_world_formation_label_cant_equip;
extern oriented_quad_t g_world_formation_label_equipping;
extern oriented_quad_t g_world_formation_label_leaving;
extern oriented_quad_t g_world_formation_unit_dot_sprite;
extern oriented_quad_t g_world_formation_label_missing;
extern menu_sprite_cell_t g_world_formation_label_tiles[];
extern menu_sprite_cell_t g_world_formation_label_tiles_bottom[];
extern s8 g_world_formation_dot_glow_timer;
extern s8 g_world_formation_dot_glow_direction;
extern u8 g_world_formation_cursor_trail_brightness[8];
extern u8 g_world_formation_grid_transition_frame;
extern u8 g_world_formation_grid_return_frame;
extern u8 g_world_formation_grid_return_done;
extern u8 g_world_formation_portrait_slide_frame;
extern rgb24_t g_world_formation_away_unit_color;
extern menu_entry_t g_world_formation_triangle_menu;
extern menu_entry_t g_world_formation_dismiss_prompt;
extern menu_entry_t g_world_formation_dismiss_confirm_menu[4];
extern menu_entry_t g_world_formation_dismiss_confirm_menu_scrolled[4];
extern world_formation_dismiss_entry_t g_world_formation_dismiss_entries[];
extern s16 g_world_formation_dismiss_prompt_text_id;
extern s16 g_world_formation_dismiss_prompt_kind;
extern s16 g_world_formation_dismiss_anim_timer;
extern s8 g_world_formation_sort_field_indices[];
extern menu_entry_t g_world_shop_hire_confirm_menu;
extern u8 g_world_shop_hire_confirm_message_wait;
extern u8 g_world_formation_thread_status_snapshot[16];
extern POLY_FT4 g_world_formation_view_quads[2][30];
extern point16_t g_world_formation_cursor_target;
/* Cursor trail position history at 0x801c8344, reset by
 * world_formation_reset_slots_and_frame_tpages; entry 7 is the newest and x is
 * -1 in an unused entry. */
extern point16_t g_world_formation_cursor_trail[8];
extern u8 g_world_formation_dismiss_step;
extern menu_entry_t* g_world_formation_dismiss_menu_data;
extern s16 g_world_formation_record_order[];
extern struct world_unit g_world_unit_records[WORLD_FORMATION_UNIT_RECORD_COUNT];
extern u8 g_world_formation_menu_text_buffer[];

/* Menu callers cache the builder's return here; rebuilding the record list
 * does not itself refresh this byte. */
extern u8 g_world_formation_unit_count;
extern world_unit_t* g_world_unit_pointers[];
void world_formation_run_screen(s32 tutorial, s32 unused);
s32 world_formation_is_unit_list_scroll_needed(void);
void world_formation_draw_unit_slots(point16_t* positions, s32 shade);

void world_formation_draw_graphics_below_sprite(
    s16 unit_index, s32 mode, s32 x, s32 y, rgb24_t* color, s32 (*get_value)(s32), s32 enabled, s32 priority);

s32 world_formation_run_equip_item_menu(void);
s32 world_formation_run_set_ability_menu(void);
void world_formation_build_job_wheel_availability(void);
void world_formation_build_job_wheel_job_name(void);
s16 world_formation_build_record_list(s32 item_id, world_unit_t** unused_list, s32 mode);

void world_formation_build_unit_graphic_entry(s16 formation_index, texture_source_t* destination, s32 unused);

void world_formation_build_unit_record(
    const struct unit_stats* unit, struct world_unit* record, const struct party_unit* party);

s32 world_formation_can_dismiss_unit(s16 formation_idx);
s32 world_formation_can_equip_item_in_slot(s16 unit_index, s16 slot, s16 item_id);
s32 world_formation_is_selected_unit_in_upper_rows(s32 scroll_position);
s32 world_formation_change_unit_job(s16 unit_id, s16 job_id, s32 update_equipment);
void world_formation_copy_unit_to_temp(s16 index, s32 unused_slot);
s32 world_formation_cursor_distance_falloff(s32 x, s32 y, s32 base, s32 floor);
void world_formation_cycle_selected_unit(s16 pad_buttons);
void world_formation_dismiss_unit(s16 formation_idx);
void world_formation_draw_background_tiles(s32 y, s32 unused_1);
void world_formation_draw_cursor_trail(point16_t* cursor);
void world_formation_draw_job_wheel_sprites(s32 angle, s32 radius_x, s32 radius_y);
void world_formation_draw_sprite_with_shadow(const oriented_quad_t* pos, const rgb24_t* rgb, s32 ot_index);

void world_formation_draw_egg_sprite(
    point16_t* pos, s32 palette, const rgb24_t* rgb, s32 hatch_countdown, s32 ot_index);

void world_formation_draw_unit_sprite(s16 unit_index, point16_t* pos, rgb24_t* rgb, s32 ot_index);
s32 world_formation_equip_item_to_unit_slot(s16 unit, s16 slot, s16 item_id);
s32 world_formation_find_index_by_roster_slot(s16 roster_slot);
s32 world_formation_generate_unit(s16 unit_type);

void world_formation_get_generic_human_graphic_entry(s32 job_id, s16 is_female, texture_source_t* destination);

s32 world_formation_get_job_wheel_cursor_job_level(s32 unused_row);
s32 world_formation_get_unit_brave(s32 unit_id);
s32 world_formation_get_unit_experience(s32 unit_id);
s32 world_formation_get_unit_faith(s32 unit_id);
s32 world_formation_get_unit_egg_hatch_total(s32 unit_id);
s32 world_formation_get_unit_egg_hatch_progress(s32 unit_id);
s32 world_formation_get_unit_level(s32 unit_id);
void world_formation_init_cursor_and_scroll_state(s32 formation_index);
void world_formation_init_menu_display(void);
void world_formation_init_menu_state(void);
void world_formation_init_selection_cursor(s16 formation_index);
s32 world_formation_is_job_wheel_selection_available(s32 unused_row);
void world_formation_item_menu(void);
void world_formation_layout_job_wheel_portrait(void);
s32 world_formation_get_unit_lock(s32 unit_id);
void world_formation_place_unit_portrait_at_rest(s16 unit_index);
s32 world_formation_process_job_change(s32 job);
void world_formation_rebuild_unit_list(void);
void world_formation_recalculate_selected_unit_stats(void);
void world_formation_remove_invalid_unit_loadout(s16 unit_id);
void world_formation_reset_menu_context(void);
void world_formation_reset_slots_and_frame_tpages(void);
void world_formation_restore_menu_after_core_exit(void);
void world_formation_restore_saved_system_flags(void);
void world_formation_run_dismiss_unit_menu_step(void);
void world_formation_run_job_wheel_menu(void);
s16 world_formation_run_menu_frame(u32* otag, u32 input);
void world_formation_run_screen_for_menu_index(s32 menu_index);
void world_formation_save_and_set_system_flags(void);
void world_formation_save_records_to_party(void);
void world_formation_set_display_mode(s32 mode);
void world_formation_set_menu_allowed_buttons(u32 mask);
void world_formation_set_ordering_table_pointer(u32* ordering_table);
s32 world_formation_set_unit_ability_slot(s16 unit_id, s16 slot, s16 ability, s32 update_equipment);
void world_formation_set_unit_name(s16 formation_index, const u8* name);
void world_formation_stage_selected_unit(void);
void world_formation_start_job_wheel_close(void);
s32 world_formation_step_grid_return_transition(s32 mode);
s32 world_formation_step_grid_transition(s32 mode);
void world_formation_step_job_wheel_close_animation(void);
void world_formation_step_job_wheel_open_animation(void);
void world_formation_step_scroll_velocity(s8* velocity, s8* position);
u8 world_formation_step_unit_portrait_slide(void);
void world_formation_step_unit_selection_slide(void);
void world_formation_stop_menu_threads(void);
/* get_mode and get_shade take the unit slot index; NULL selects the default. */
s32 world_formation_update_and_draw_unit_grid(s16 background_y, s16 toggle_banner, s16 allow_browse, s16 input,
    s16 locked, s32 (*get_mode)(s32), s32 (*get_shade)(s32));
void world_formation_update_job_wheel_state(void);
void world_formation_update_open_request(void);
void world_formation_update_unit_selection(void);
void world_formation_view_zoom_thread(void);
void world_formation_update_tutorial_categories_request(void);
void world_formation_update_debug_variable_list_request(void);
void world_formation_update_fixed_number_panel_request(void);
void world_formation_update_world_map_root_request(void);
void world_formation_sort_roster(s32 index);
s32 world_get_formation_unit_stat_by_mode(s32 unit_index);

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

extern menu_status_panel_buffer_t g_world_stat_preview_panel_frames[2];

extern struct menu_status_panel_slot_storage g_world_selected_unit_stat_detail;
extern const world_unit_monster_breed_entry_t g_world_unit_monster_breed_table[WORLD_MONSTER_BREED_ENTRY_COUNT];

extern menu_unit_status_identity_t g_world_selected_unit_identity;
extern CVECTOR g_world_unit_panel_bar_colors[];
extern menu_number_entry_t g_world_selected_unit_number_entries[];
extern image_load_parameters_t g_world_unit_summary_label_sprite_params[];
extern image_load_parameters_t g_world_unit_summary_value_sprite_params[];
extern RECT g_world_unit_status_panel_origin;
extern image_load_parameters_t g_world_unit_status_sprite_params[];
extern image_load_parameters_t g_world_unit_status_portrait_params;
extern u16 g_world_unit_summary_value_cluts[4][2]; /* [row][dimmed] */
extern u16 g_world_unit_summary_label_cluts[7][2]; /* [row][dimmed] */
extern image_load_parameters_t g_world_unit_summary_mode_row_params[4];
extern RECT g_world_selected_unit_portrait_rect;
extern menu_status_thread_t g_world_stat_preview_panel_thread_params;
extern menu_text_binding_t g_world_selected_unit_text_binding;
extern u8 g_world_unit_status_banner_active;
extern menu_entry_t g_world_unit_status_banner_menu;
extern s8 g_world_unit_status_banner_state; /* banner state: 0 idle, 1 finishing, else countdown */

/* Panel scratch aliases inside g_thread_contexts[0] and [14], at
 * +0x100 and +0x200 in each slot's stack. These stay separate symbols;
 * the palette areas hold one slot per page. */
extern menu_status_panel_editor_packet_t g_world_selected_unit_summary_packets[2];
extern menu_status_panel_packet_t g_world_selected_unit_status_packets[2];
extern u8 g_world_selected_unit_stat_text_image[];
extern u8 g_world_selected_unit_name_text_image[];
extern menu_unit_number_images_t g_world_selected_unit_number_text_images;
extern palette_t g_world_selected_unit_portrait_palettes[2];
extern u8 g_world_stat_preview_text_image[];
s32 world_get_party_unit_formation_sprite(s32 party_index);
void resolve_unit_status_changes(void);
s32 unit_has_adjacent_monster_skill_support(struct unit_stats* unit);
void world_unit_build_best_fit_equipment(s16 unit_index, s16* equipment);
s32 world_unit_can_equip_item_id(s16 unit_index, s16 item_id);
void unit_copy_map_coords_by_runtime_id(u32 runtime_id, SVECTOR* coords);
s32 world_formation_get_unit_palette_index(s32 roster_slot);
s32 world_unit_get_zodiac_for_date(s32 month, s32 day);

void world_unit_update_monster_breeding(s32 unused);

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

extern u8* g_world_text_resident_data[9];

extern text_glyph_state_t g_world_text_message_box_glyph;
extern glyph_blit_t g_world_text_message_box_state;
extern s32 g_world_text_section_offsets[32];

extern s16 g_world_fixed_number_panel_request_state;
extern s32 g_world_text_message_box_digit_divisors[];
extern s32 g_world_text_message_box_slot_threads[3];
extern s32 g_world_text_message_box_slot_glyph_counters[3];
extern s32 g_world_text_message_box_glyph_delay;
extern u8 g_world_text_message_box_slot_carry_offset[4];
extern u8 g_world_text_message_box_slot_prev_width[4];

extern u8 g_world_text_task_phase;
extern u8 g_world_text_skillset_names_data[];
extern u8 g_world_text_job_names_data[];
extern u8 g_world_text_ability_names_data[];
extern u8 g_world_text_formation_messages_data[];
extern u8 g_world_text_dismiss_unit_replies_data[];
extern u8 g_world_text_job_descriptions_data[];
extern u8 g_world_text_name_entry_keyboard_data[];
extern u8 g_world_text_shop_dialogue_data[];
extern u8 g_world_text_shop_menu_labels_data[];
extern u8 g_world_text_name_entry_keys_data[];
extern SVECTOR g_world_text_partner_screen_coords;
extern SVECTOR g_unit_scratch_coords;
extern s32 g_world_text_substitution_values_backup[32]; /* 32-bit substitution values saved around the text calls */
extern menu_list_page_t g_world_text_list_pages[2];

extern RECT g_world_text_message_box_vram_rect;
extern u8 g_world_text_message_box_glyph_images[3][8][84];
extern u8 g_world_text_message_box_previous_images[3][84];
extern u8 g_world_text_message_box_column_scratch[14];
extern u8 g_world_text_row_image_buffer[];

u8* world_text_get_resident_data(world_text_data_e index);
u8* world_text_bind_section_data(u32 index);

void world_text_message_box_thread(void);
void world_text_message_box_draw_glyph_with_typewriter_delay(s32 x, s32 y, s32 wait);

void world_text_render_id_list_to_image_rows(u8* font, s16* text_ids, RECT* destination, s32 palette);

u8* world_text_advance_entry_by_char_count(u8* text, s32 count);
/* MAIN selects the WORLD address of text_find_entry through this existing alias. */
u8* world_text_find_entry(u32 text_id);
void world_text_generate_formation_unit_name_string(void);
s16 world_text_get_encoded_char_at_position(u8* text, s16 position, s32 mode);
s32 world_text_is_printing(s32 thread_id);
void world_text_kern_into_command_buffer(u8* text, s16* name, RECT* rect, s32 palette);
s32 world_text_measure_entry_pixel_width(const u8* entry);
void world_text_resume_printing(s32 thread_id);
void world_text_show_message_and_play_sound(s32 message_id, s32 sound_id);

/* menu context */
/* Ordinary outfitter screens use a town's world_location_id_e. These values
 * identify the other screens that reuse the formation menu framework. */
typedef enum world_menu_context {
    WORLD_MENU_CONTEXT_NAME_ENTRY = -2,
    WORLD_MENU_CONTEXT_FORMATION = -1,
    WORLD_MENU_CONTEXT_FUR_SHOP = 100,
    WORLD_MENU_CONTEXT_SOLDIER_OFFICE = 101,
} world_menu_context_e;

extern s16 g_world_menu_context_id; /* town location ID or world_menu_context_e */

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

/* Index of a saved cursor in g_world_menu_cursor_positions, as the formation
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

extern u16 g_world_menu_clut;
extern s32 g_world_menu_color_blue;
extern s32 g_world_menu_color_green;
extern s32 g_world_menu_color_red;
extern s32 g_world_menu_event_state_flag;
extern s32 g_world_menu_main_scroll_direction;
extern s32 g_world_menu_main_scroll_threshold;
extern s16 g_world_menu_order_unit_cursor;
extern s16 g_world_menu_order_unit_entries[];
extern s8 g_world_menu_order_unit_open;
extern u16 g_world_menu_palette_clut;
extern u8 g_world_menu_prompt_active;
extern const RECT g_world_option_gfx_draw_area_template;
extern s16 g_world_menu_row_height;
extern s16 g_world_menu_screen_open_request;
extern s32 g_world_menu_scroll_row_offset;
extern rgb24_t g_world_menu_sprite_color;
extern s32 g_world_menu_text_file_buffer[];
/* Provisional: menu text origin/stride fields at 0x80153280 (the WORLD
 * script variable region; menu_set_text_origin writes +0x04/+0x06).
 * The text-image builders address these through one base register, so they
 * are modeled as one aggregate; the two 32-bit parameters at 0x80153304 and
 * 0x80153308 schedule as plain scalars and stay separate globals. */
extern u16 g_world_menu_texture_page;
extern s32 g_world_menu_thread_result;
extern s32 g_world_menu_use_alternate_main_palette;
extern u8 g_world_menu_use_alternate_palette;
extern s16 g_world_menu_use_scroll_position;
extern s32 g_world_menu_scrolling_list_depth;

/* Four image-load parameter blocks per icon kind, for kinds 5..15. */
extern image_load_parameters_t g_world_menu_icon_strip_image_params[];

/* One image-load parameter block per single-icon kind 0..4; index 1 doubles as
 * the arrow placement used by kind 0x10. */
extern image_load_parameters_t g_world_menu_single_icon_image_params[];

extern s16 g_world_menu_open_entry_index;
extern world_menu_thread_group_t g_world_menu_thread_groups[];
extern s32 g_world_menu_thread_group_parameter;
extern s32 g_world_menu_location_id;
extern s32 g_world_menu_location_window_x;
extern s32 g_world_menu_location_window_y;
extern u32 g_world_menu_default_entry_0_text_values[2]; /* text index + 0xa000, value */
extern s32 g_world_menu_default_entry_1_text_values[2]; /* value, 0 */
extern s32 g_world_menu_location_text_id;
extern u8 g_world_menu_submenu_page_counts[];
extern u8* g_world_menu_submenu_page_text_ids[];
extern menu_scroll_text_layout_t g_world_menu_submenu_page_layout;
extern s32 g_world_menu_number_entry_value;
extern menu_entry_t g_world_menu_default_entries[];
extern RECT g_world_menu_frame_palette_rect;
extern RECT g_world_menu_frame_palette_tail_rect;
extern RECT g_world_menu_frame_image_rect;
extern RECT g_world_menu_item_image_rect;
extern RECT g_world_menu_common_clut_rect;
extern RECT g_world_menu_item_palette_rect;
extern RECT g_world_menu_unit_image_rect_a;
extern RECT g_world_menu_unit_image_rect_b;
extern clut_t g_world_menu_common_clut_data;
extern const u16 g_world_menu_zoom_percentages[12]; /* twelve crop percentages, indexed by step */
extern s16* g_world_menu_entry_text_id_tables[];    /* 16-bit tables selected by entry select_text_table */
extern s32 g_world_menu_number_entry_digit_steps[];

/* Per-piece 4.12 fixed-point x/y scale; a negative factor flips that axis. */
extern menu_button_piece_t g_world_menu_button_pieces[];
extern point16_t g_world_menu_button_piece_scales[];
extern u16 g_world_menu_window_tpage;
extern u16 g_world_menu_palette_clut_normal;
extern u16 g_world_menu_palette_clut_alt;
extern u16 g_world_menu_alternate_number_clut_alt;
extern u16 g_world_menu_alternate_number_clut_normal;
extern u16 g_world_menu_disabled_number_clut_normal;
extern u16 g_world_menu_disabled_number_clut_alt;
extern u16 g_world_menu_hp_gauge_clut;
extern u16 g_world_menu_mp_gauge_clut;
extern u16 g_world_menu_ct_gauge_clut;
extern u16 g_world_menu_background_clut;
extern u16 g_world_menu_window_clut_alt;
extern u16 g_world_menu_window_clut_normal;
enum { WORLD_MENU_SCRIPT_END = 0x1c };

extern menu_sprite_command_t g_world_menu_script_sprite_command;
extern menu_window_command_t g_world_menu_window_interior_command;
extern s16 g_world_menu_window_scale_table[];
extern oriented_quad_t g_world_menu_number_blank_sprite;
extern s8 g_world_menu_cursor_bob_thresholds[];
extern s8 g_world_menu_cursor_bob_active_thresholds[];
extern s16 g_world_menu_list_row_text_ids[];
extern u16 g_world_menu_list_cursor_anim;
extern u16 g_world_menu_scroll_hold_counter;
extern s16 g_world_menu_text_redraw_request;
extern world_menu_grid_cell_t g_world_grid_menu_default_cells[];
extern world_menu_grid_cell_t g_world_grid_menu_shop_cells[];
extern world_menu_grid_cell_t g_world_grid_menu_shop_scrolled_cells[];
extern world_menu_grid_cell_t* g_world_active_grid_menu_cells;
extern u16 g_world_grid_menu_cursor_anim; /* grid cursor animation state */
extern u8 g_world_grid_menu_initialized;  /* grid menu initialized */
extern s16 g_world_menu_at_list_flags[256];
extern battle_at_entry_t g_world_action_target_list_slots[BATTLE_AT_LIST_CAPACITY];
extern s16 g_world_menu_submenu_page_row_text_ids[];
extern u32* g_world_menu_list_controller_input;
extern u16 g_world_menu_scroll_repeat_timer;
extern s16 g_world_menu_list_visible_rows;
extern s16 g_world_menu_list_row_record_count;
extern s16 g_world_menu_window_right_x;
extern s8 g_world_menu_list_layout_pending;
extern const s16* g_world_menu_list_entry_ids;
extern u8* g_world_menu_list_text_table;
extern u16 g_world_menu_list_image_width;
extern u8 g_world_menu_list_glyph_width;
extern u8 g_world_menu_list_text_columns;
extern s8 g_world_menu_list_scroll_direction;
extern RECT g_world_menu_window_rect;
extern u8 g_world_grid_menu_selected_cell; /* selected cell */
extern u16 g_world_menu_alternate_number_clut;
extern s8 g_world_menu_music_slot; /* formation music slot */
extern u16 g_world_menu_window_clut;
extern u16 g_world_menu_active_clut_front;
extern u16 g_world_menu_active_clut_back;

/* Result slot per menu (-1 while none); world_menu_reset_selection_results clears all 16. */
extern s16 g_world_menu_selection_results[16];
extern s32 g_world_grid_menu_id; /* menu_grid_help_id_e */

extern menu_callback_t g_world_menu_script_callbacks[16];
extern u16 g_world_menu_disabled_number_clut;

/*
 * WORLD.BIN routines shared by the world, wldcore, and menu-overlay
 * reconstructions.  Each module's linker table maps these names to its own
 * copy of the routine.
 */
menu_instruction_t* world_menu_parse_draw_window_frame(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_conditional(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_if_else(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_draw_tile(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_draw_sprite(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_draw_callback_sprite(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_draw_line(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_set_fixed_clut(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_set_clut(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_set_sprite_color(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_set_texture_page(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_set_semi_trans(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_set_draw_priority(menu_instruction_t* instruction);
menu_instruction_t* world_menu_parse_draw_tiled_rectangle(menu_instruction_t* instruction);
menu_instruction_t* world_menu_handle_window_command_with_scaled_clip(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_clear_temporary_draw_area(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_set_draw_area(menu_instruction_t* instruction);
menu_instruction_t* world_menu_script_add_blend_draw_mode(menu_instruction_t* instruction);
menu_instruction_t* world_menu_skip_instruction_group(menu_instruction_t* instruction);

void world_menu_number_entry_thread(void);
void world_menu_run_script_with_callback_suppressed(void);
void world_menu_open_name_entry_key_list(void);
void world_menu_labeled_number_entry_thread(void);
void world_menu_animate_window_quad_crop_full(menu_window_buffer_t* buffers, RECT* rect, RECT* unused_to);
void world_menu_animate_window_quads_to_rects(menu_window_buffer_t* buffers, RECT* to_main, RECT* to_icon);

void world_menu_draw_pressable_button(s16 index, s32 x, s16 y, s16 pressed, s16 disabled, u16 sound_effect_id);
void world_screen_menu_draw_text_columns(menu_entry_t* entry, s32* row_offset, u8* buffer);
s32 menu_get_party_unit_name_id(void);
s32 world_menu_get_selected_unit_hp_bonus(s32 unused_row);
s32 world_menu_get_selected_unit_mp_bonus(s32 unused_row);
s32 world_menu_is_busy(s32 unused);
void world_menu_open_entry_window(s32 index, s32 unused_x, s32 unused_y);
u8* world_screen_menu_redraw_text_page_on_scroll(menu_entry_t* entry, s32* row_offset, s32* redraw, s32* unused_cursor);

u8* world_menu_redraw_text_page_on_scroll_in_rect(
    menu_entry_t* entry, s32* row_offset, s32* redraw, s32* unused, RECT* area);

void world_menu_render_text_image_at_fixed_origin(text_dialog_record_t* record);
void world_screen_menu_window_frame_thread(void);
void world_menu_run_order_unit(void);
s32 world_menu_run_unit_status_banner_countdown(s32 message_id, s32 frames);
void world_menu_add_gradient_line_primitive(point16_t* endpoints, rgb24_t* colors, s32 semi_trans, s32 ot_index);
void world_menu_announce_entry_value_thread(void);
void world_menu_resize_parent_entry_to_digits(void);
void world_menu_build_layout_sprites(menu_entry_t* entry, SPRT sprite[2], s32 unused_2);
s32 world_menu_build_window_frame_image(s32 width, s32 height, u16* destination);

void world_menu_draw_row_window_and_cursor(s32 wide, s32 row, s32 mode);
void world_menu_enable_all_order_entries(void);
void world_screen_menu_build_window_image_row(const u16* source, u16* dest, s32 count, s32 offset);
s32 world_menu_get_event_state_flag(void);
s32 world_menu_get_item_half_price_display_value(s32 entry_index);
s32 world_menu_get_item_price_display_value(s32 entry_index);
s32 world_menu_get_learn_job_jp(s32 index);
void world_menu_init_quad_from_record(RECT* out, const menu_entry_t* record, SPRT* primitive);
s32 world_menu_handle_remove_all_equipment(void);
s32 world_menu_has_items(s32 mode);
void world_menu_init_and_load_scrollable_list(
    const s16* values, s32 selected_index, u8* text_table, menu_instruction_t* script);
void world_option_gfx_init_scaled_draw_area_packets(scaled_draw_area_pair_t* pair);
s32 world_menu_is_learn_ability_learned(s32 index);
s32 world_screen_menu_lerp_fixed12(s32 from, s32 to, s32 t);
void world_menu_load_common_graphics(s32 load_extra_pages);
void world_menu_load_scrollable_list_layout(menu_instruction_t* script);
void world_menu_load_text_1bd8(void);
void world_menu_load_text_1bd8_with_face(void);
void world_menu_load_text_1c18(void);
void world_menu_reset_selection_results(void);
void world_menu_run_ability(void);
s32 world_menu_run_best_fit_equipment(void);
s32 world_menu_run_learn(void);
s32 world_menu_run_learn_job_list(void);
void world_menu_run_main_mode(s32 mode);
void world_menu_run_main_mode_0_thread(void);
s32 world_menu_run_remove_ability(void);
s32 world_menu_run_remove_item(void);
void world_menu_select_stat_gauge_level(s32 x, s32 y, s32 otag_index, s32 value);
void world_menu_set_brightness(s32 red, s32 green, s32 blue);
s16 menu_init_cursor_position(u8 index, s16 value);
void world_menu_set_default_entry_0_text_values(u32 text_index, u32 value);
void world_menu_set_default_entry_1_text_values(s32 value);
void world_menu_set_palette_mode(s32 mode);
void world_menu_set_sprite_color(const rgb24_t* src);
void world_menu_start_description_text_thread(s32 position);
void world_menu_start_thread_group(s32 parameter, s32 location_id, s32 window_x, s32 window_y);
void world_menu_switch_item_category(s32 input);
void world_main_menu_text_window_thread(void);
void world_menu_thread_group_supervisor_thread(void);
void world_menu_toggle_preview_stats_window(s32 flag);
void menu_toggle_change_banner_panel_thread(s32 enable);
void world_menu_toggle_ability_panel_thread(s32 enable);
void menu_toggle_stat_preview_panel_thread(s32 enable);
void world_menu_start_equipment_and_ability_panel_thread(s32 enabled);
s32 world_menu_update_clear_all_abilities_prompt(void);

void world_menu_update_entry_cursor_quads(
    menu_entry_t* entry, POLY_FT4* quad, POLY_FT4* shadow, s32 unused, s32 cursor);

void world_menu_update_grid_cursor(s16 alternate);

void world_menu_update_thread_7_idle_countdown(void);
void world_menu_set_slot_4_cursor_index(s32 index);

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

extern card_save_buffer_t* g_world_load_work_buffer;
extern s16 g_world_card_list_scroll_velocity;
extern u8 g_world_card_menu_step; /* world_card_menu_step_e */
extern s32 g_world_card_open_descriptor;
extern card_save_icon_t g_world_card_save_icon_records[15];
extern u8 g_world_card_save_title_template[0x16];
extern u8 g_world_card_slot_cursor_row;
extern s16 g_world_card_menu_request_state; /* world_screen_request_state_e */
extern const char* g_world_card_save_file_names[15];

/* Encoded captions addressed directly by world_card_build_save_slot_description,
 * immediately after the 15 save-file pointers at g_world_card_save_file_names. */
extern u8 g_world_card_slot_text_table[];
extern s16 g_world_card_slot_select_timer;
extern RECT g_world_card_screen_background_tile;
extern menu_entry_t g_world_card_slot_select_menu[2];
extern u8 g_world_card_slot_select_running;

/* Message entries selected by world_card_run_mode_select_step. */
extern menu_entry_t g_world_card_save_unavailable_message;
extern menu_entry_t g_world_card_no_save_data_message;
extern menu_entry_t g_world_card_mode_select_menu;
extern u8 g_world_card_mode_select_running;

/* A pointer cell, not a record: cleared to 0, tested, and assigned
 * g_world_card_save_unavailable_message / g_world_card_no_save_data_message by world_card_run_mode_select_step. */
extern menu_entry_t* g_world_card_pending_message;
extern menu_entry_t g_world_card_saving_message;
extern menu_entry_t g_world_card_loading_message;
extern menu_entry_t g_world_card_checking_message;
extern menu_entry_t g_world_card_load_error_message;
extern menu_entry_t g_world_card_save_error_message;
extern u8 g_world_card_load_step_state;
extern u8 g_world_card_save_step_state;
/* Two-entry confirmation chains; the choice is entry 1 selected_index. */
extern menu_entry_t g_world_card_format_prompt[2];
extern menu_entry_t g_world_card_overwrite_confirm_prompt[2];
extern menu_entry_t g_world_card_load_confirm_prompt[2];
extern menu_entry_t* g_world_card_confirm_prompt;
extern menu_entry_t g_world_card_formatting_message;
extern menu_entry_t g_world_card_access_error_message;
extern s8 g_world_card_slot_scan_active;
extern s8 g_world_card_format_delay_counter;
extern s8 g_world_card_error_message_closing;
extern menu_window_command_t g_world_card_slot_window_command;
extern menu_window_rect_source_t g_world_card_slot_window;
extern s16 g_world_card_slot_description_row_ids[];
extern menu_draw_number_command_t g_world_card_slot_number_command;
extern menu_draw_number_command_t g_world_card_playtime_hours_command;
extern menu_draw_number_command_t g_world_card_playtime_minutes_command;
extern menu_draw_number_command_t g_world_card_playtime_seconds_command;
extern point16_t g_world_card_slot_cursor_point;
extern u16 g_world_card_slot_cursor_anim;
extern u8 g_world_card_slot_list_input_ready;
extern u8 g_world_card_overwrite_prompt_running;
extern u8 g_world_card_load_prompt_running;
extern oriented_quad_t g_world_card_slot_quad_0;
extern oriented_quad_t g_world_card_slot_quad_1;
extern oriented_quad_t g_world_card_slot_quad_2;
extern oriented_quad_t g_world_card_slot_quad_3;
extern oriented_quad_t g_world_card_slot_quad_4;
extern u8 g_world_card_load_failed;
extern u8 g_world_card_save_failed;
extern s8 g_world_card_probe_result;
extern s8 g_world_card_scan_prompt_active;
extern s8 g_world_card_slot_scan_index;
extern s8 g_world_card_free_block_count;
extern s8 g_world_card_save_file_count;
extern s8 g_world_card_scan_start_frame;
extern u8 g_world_card_save_mode;
extern u8 g_world_card_menu_result;
extern s16 g_world_card_list_scroll_y;
extern s16 g_world_card_mode_select_enabled;
extern s16 g_world_card_active_slot;
extern s16 g_world_card_list_first_visible_row;
extern struct DIRENTRY* g_world_card_directory_buffer;
extern s8 g_world_card_selectable_slot_count;
extern u8 g_world_card_save_slot_descriptions[][0x8C];
s32 world_card_build_save_slot_description(s32 mode, u8* destination);
s32 world_card_read_file_chunked(const char* filename, u8* destination, u32 size);
void world_card_build_save_file_header(s32 slot, s32 level, struct card_save_header* header);
s32 card_file_close_with_retries(s32 descriptor);
s32 world_card_count_free_blocks(const struct DIRENTRY* entries, s32 count);
s32 world_card_count_save_files(struct DIRENTRY* dir, s32 all);
s32 world_card_count_selectable_save_slots(void);
s32 card_io_create_new_with_retries(s32 port, s32 retry_limit);
s32 world_card_delete_file(const char* name);
void world_card_draw_slot_text(s32 slot);
s32 world_card_format_selected_slot(void);
s32 world_card_get_selected_slot(void);
void world_card_init_menu_state(void);
s32 world_card_load_globals_from_save_image(s32 partial_load);
s32 world_card_load_with_retries(s32 channel, s32 attempts);
s32 world_card_open_file_with_retries(const char* filename, s32 mode);
s32 world_card_probe_selected(void);
s32 card_file_read_with_retries(s32 descriptor, void* destination, s32 size);
void world_card_run_load_step(s32 slot);
s32 world_card_run_menu_screen(s32 mode_select_enabled);
void world_card_run_mode_select_step(void);
void world_card_run_save_step(s32 slot);
void world_card_run_slot_select_step(void);
void world_card_scroll_menu_list(s32 row);
void world_card_update_and_draw_slot_list(s32 input);
void world_card_update_menu_request(void);
void world_card_update_save_slot_playtime(s32 slot);
void world_card_update_slot_scan(void);
s32 world_card_wait_for_selected_status(s32 attempts);
s32 card_file_write_with_retries(s32 descriptor, const void* source, s32 size);
void world_card_build_save_image(s32 slot);
s32 card_file_seek_with_retries(s32 descriptor, s32 offset, s32 origin);
s32 world_card_write_buffer_to_file(const char* filename, const card_save_buffer_t* source, u32 size, s32 create_file);

/* item */

extern oriented_quad_t g_world_item_category_tab_cursor_sprite;
extern s16 g_gfx_item_icon_clut_base_x; /* item icon CLUT base x */
extern s16 g_gfx_item_icon_clut_base_y; /* item icon CLUT base y */

/* Menu-subflow "running" flags. The readers sign-extend (lb), so these are
 * signed; the initializer only stores 0/1/2 and cannot distinguish. */
extern s8 g_world_item_menu_initialized;
extern s8 g_world_item_best_remove_initialized;
extern s8 g_world_item_submenu_initialized;
extern s8 g_world_item_category_menu_initialized;
extern s8 g_world_item_action_menu_initialized;
extern s8 g_world_item_sort_order_initialized;
extern s8 g_world_item_equipping_units_initialized;
extern menu_entry_t g_world_item_icon_menu;
extern u8 g_world_item_menu_unit_index;
extern u8 g_world_item_submenu_exit_pending;
extern s8 g_world_remove_item_unit_index;
extern s16 g_world_remove_item_saved_primary_input;
extern s16 g_world_remove_item_saved_secondary_input;
extern u16 g_world_remove_item_saved_newly_pressed;
extern world_menu_number_range_t g_world_item_action_quantity_range;
extern s16 g_world_item_action_option_states[3];
extern u16 g_world_remove_item_cursor_anim;
extern point16_t g_world_remove_item_cursor_point;
extern menu_entry_t g_world_item_quantity_change_menu;
extern menu_entry_t g_world_item_unequip_all_menu;
extern menu_instruction_t g_world_item_list_window_script;
extern menu_instruction_t g_world_item_hp_mp_bonus_window_script;
extern s8 g_world_item_menu_mode; /* menu_item_list_state_e */

/* The two item-category icon strips; the active one is kept in g_world_item_category_icon_strip. */
extern menu_entry_t g_world_item_category_icon_strip_a;
extern menu_entry_t g_world_item_category_icon_strip_b;
extern menu_instruction_t g_world_item_category_tabs_script;      /* menu display stream (narrow variant) */
extern menu_instruction_t g_world_item_category_tabs_wide_script; /* menu display stream (wide variant) */
extern oriented_quad_t g_world_item_category_tab_sprites[];       /* 20-byte quads, one per row */
extern u8 g_world_item_category_tab_x[];                          /* per-row base x */
extern u8 g_world_item_category_input_lock;
extern u8 g_world_item_menu_saved_browse_enabled;
extern s16 g_world_remove_item_equipment[WORLD_EQUIPMENT_SLOT_COUNT];
extern s8 g_world_item_category_previewed_cursor;
extern s16 g_world_item_category_strip_text_ids[]; /* option text ids of the active strip, -1 terminated */
extern menu_entry_t* g_world_item_category_icon_strip;
extern u16 g_world_item_saved_selected_unit_index;
s32 world_item_build_category_list(s16 unit_id, s16 sort_mode, s8 category, s16* entries, u8 mode);

void world_item_cash_out_excess_inventory(void);

s32 world_item_count_selected_equipped_by_party(s32 unused_row);
s32 world_item_count_selected_owned_and_equipped(s32 unused_row);
s32 world_item_get_equip_candidate_brightness(s32 unit_index);
s32 world_item_get_equip_candidate_marker(s32 unit_index);

/* WORLD menu helpers. Half-price is clamped to one; stock to a byte. */
s32 world_item_get_half_price(s16 item_id);
s32 world_item_get_price(s16 item_id);
s32 world_item_get_selected_half_price(s32 unused_row);

s8 world_item_run_action_menu(void);
s8 world_item_run_sort_order_menu(void);
world_unit_equipment_slot_e world_item_select_equipment_slot(s16 unit_index, s32 item_id);
s32 world_item_show_units_equipping_selected(void);

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

extern s16 g_world_shop_fitting_room_items[PARTY_ROSTER_SLOT_COUNT][WORLD_EQUIPMENT_SLOT_COUNT];
extern u8 g_world_shop_hire_menu_initialized;
extern u8 g_world_shop_item_category;
extern world_shop_item_availability_t g_world_shop_item_availability[];
extern void (*g_world_shop_menu_step_handlers[])(void);
extern s16 g_world_shop_menu_step; /* world_shop_step_e */
extern s16 g_world_shop_quantity_limit;
extern menu_entry_t g_world_shop_quantity_menu_data[2];
extern u8 g_world_shop_background_visible;
extern menu_entry_t g_world_shop_main_menu_thread_data;
extern menu_entry_t g_world_shop_fur_menu_thread_data;

/* The cursor doubles as the soldier-office mode (world_shop_get_soldier_office_entry_fee). */
extern menu_entry_t g_world_shop_hire_menu_thread_data;
extern RECT g_world_shop_background_rect;
extern RECT g_world_shop_backdrop_tile_rect;
extern RECT g_world_shop_edge_tile_rect;
extern RECT g_world_shop_background_vram_rect;
extern RECT g_world_shop_vram_backup_rect;
extern u8 g_world_shop_main_menu_running;     /* menu open flag */
extern u8 g_world_shop_exit_fade_started;     /* fade-out started */
extern u8 g_world_shop_exit_step_initialized; /* state initialized */
extern menu_instruction_t g_world_shop_obtain_gil_menu_script;
extern menu_instruction_t g_world_shop_purchase_menu_script;
extern s8 g_world_shop_purchase_list_initialized;
extern s8 g_world_shop_purchase_equip_check_pending;
extern s16 g_world_shop_purchase_message_wait;
extern u8 g_world_shop_quantity_menu_running;
extern u8 g_world_shop_equip_candidate_initialized;
extern u8 g_world_shop_equip_candidate_preview_visible;
extern menu_instruction_t g_world_shop_sell_list_menu_script;
extern s8 g_world_shop_sell_list_initialized;
extern s16 g_world_shop_sell_message_wait;
extern u8 g_world_shop_sell_result; /* sell-menu thread result */
extern u8 g_world_shop_fitting_room_initialized;

/* Two-entry menu chain: row 3 of [0] opens [1] through [0]'s parent-index
 * table at 0x80195518. */
extern menu_entry_t g_world_shop_fitting_room_menu_thread_data[2];
extern menu_instruction_t g_world_shop_fitting_item_list_menu_script;
extern s8 g_world_shop_fitting_equip_menu_initialized;
extern s8 g_world_shop_fitting_equip_slot;
extern s8 g_world_shop_fitting_item_list_close;
extern u16 g_world_shop_fitting_slot_cursor_anim;
extern point16_t g_world_shop_fitting_slot_cursor_point;
extern menu_instruction_t g_world_shop_fitting_cost_menu_script;
extern s8 g_world_shop_equip_slot_preview_initialized;
extern u16 g_world_shop_equip_slot_cursor_anim;
extern point16_t g_world_shop_equip_slot_cursor_point;
extern menu_entry_t g_world_shop_checkout_confirm_menu;
extern menu_entry_t g_world_shop_checkout_short_gil_menu;
extern u8 g_world_shop_checkout_prompt_initialized;

/* The best-fit confirmation menu entry, followed directly by its open/closed
 * gate byte, as g_world_shop_sell_equipped_menu / _open are paired. */
extern menu_entry_t g_world_shop_best_fit_menu;
extern s8 g_world_shop_best_fit_menu_open;
extern menu_entry_t g_world_shop_sell_equipped_menu;
extern u8 g_world_shop_sell_equipped_menu_open;
extern u16 g_world_shop_sell_equipped_cursor_anim;
extern point16_t g_world_shop_sell_equipped_cursor_point;
extern u8 g_world_shop_sell_subflow_initialized;
extern u8 g_world_shop_fur_opened; /* fur shop opened */
extern menu_instruction_t g_world_shop_sale_confirm_menu_script;
extern s8 g_world_shop_fur_sell_list_initialized;
extern s16 g_world_shop_fur_sale_message_wait;
extern u8 g_world_shop_fur_quantity_menu_running; /* sell-menu thread result */
extern menu_instruction_t g_world_shop_fur_purchase_menu_script;
extern s8 g_world_shop_fur_purchase_list_initialized;
extern s8 g_world_shop_fur_equip_check_pending;
extern s16 g_world_shop_fur_purchase_message_wait;
extern u8 g_world_shop_fur_purchase_result; /* quantity-menu thread result */
extern menu_entry_t g_world_shop_hire_fee_confirm_menu;
extern u8 g_world_shop_recruit_preview_initialized;
extern u8 g_world_shop_hire_name_entry_initialized;
extern u8 g_world_shop_hire_fee_message_wait;
extern u8 g_world_shop_rename_prompt_initialized;
extern u8 g_world_shop_rename_name_entry_initialized;
extern menu_instruction_t g_world_shop_soldier_office_fee_menu_script;
extern world_shop_roster_count_menu_script_t g_world_shop_roster_count_menu_script;
extern s32 g_world_shop_purchase_unit_price;
extern s16 g_world_shop_purchase_item;
extern u8 g_world_shop_purchase_total_shown;
extern s16 g_world_shop_equip_candidate_last_unit;
extern u8 g_world_shop_equip_candidate_saved_browse;
extern s16 g_world_shop_equip_candidate_item;
extern u16 g_world_shop_sell_item;         /* item being sold */
extern u8 g_world_shop_sell_confirm_shown; /* confirmation prompt shown */
extern s32 g_world_shop_fitting_room_cost;
extern u8 g_world_shop_fitting_room_unit_chosen;
extern s8 g_world_shop_fitting_room_item_list_open;
extern s16 g_world_shop_fitting_room_previewed_cursor;

/* Last equipment slot whose stat preview was built, -1 when none. The
 * ATTACK overlay has an unrelated 16-bit value at this same address. */
extern s8 g_world_shop_previewed_equip_slot;
extern s16 g_world_shop_previewed_unit_index;
extern u8 g_world_shop_checkout_complete;
extern u8 g_world_shop_checkout_insufficient_gil;
extern u8 g_world_shop_checkout_prompt_active;
extern u8 g_world_shop_checkout_prompt_result;
extern menu_entry_t* g_world_shop_checkout_prompt_menu;
extern u8 g_world_shop_sell_equipped_prompt_active;
extern u8 g_world_shop_sell_equipped_slot;
extern u8 g_world_shop_sell_equipped_result_shown;
extern s16 g_world_shop_sell_subflow_step;         /* world_shop_step_e: SELL_LIST or SELL_CONFIRM */
extern u8 g_world_shop_fur_waiting_message;        /* waiting for message dismissal */
extern u8 g_world_shop_fur_has_secret_hunt;        /* any unit has Secret Hunt */
extern u8 g_world_shop_fur_has_poached_stock;      /* any poached item in stock */
extern u16 g_world_shop_sell_quantity_item;        /* item being sold */
extern u8 g_world_shop_sell_total_shown;           /* confirmation prompt shown */
extern s32 g_world_shop_fur_purchase_price;        /* half the item price */
extern u16 g_world_shop_fur_purchase_item;         /* item being bought */
extern u8 g_world_shop_fur_purchase_confirm_shown; /* confirmation prompt shown */
extern s8 g_world_shop_fur_access_granted;
extern u8 g_world_shop_hire_result_shown;
extern u8 g_world_shop_rename_error_shown;
extern u8 g_world_shop_party_unit_count;
extern const u8* g_world_shop_entered_unit_name;
extern u32 g_world_shop_service_fee;
extern u8 g_world_shop_cost_window_visible;
void world_shop_run_item_sell_list_step(void);
void world_shop_run_item_sell_confirm_step(void);
s32 world_shop_rename_unit_get_unit_marker(s32 unused_unit_index);
s32 world_shop_add_fitting_room_cost(s32 delta);
s32 world_shop_adjust_poached_item_quantity(s16 item_id, s32 delta);
void world_shop_apply_fitting_room_items_to_all_units(void);
void world_shop_build_fitting_room_best_fit_equipment(s16 unit_index, s16 shop_id, s16* out);
s32 world_shop_build_item_list(s16 unit_id, s16 shop_id, s16 category, s16* entries, s32 mode);
void world_shop_buy_from_fitting_room(void);
void world_shop_finalize_unit_equips_from_fitting_room(s16 formation_index, s16 equipment_index);
s32 world_shop_fitting_room_get_unit_marker(s32 unused_unit_index);
s32 world_shop_get_gil_minus_fitting_room_cost(s32 unused_row);
s32 world_shop_get_selected_poached_item_quantity(s32 unused_row);
void world_shop_install_callbacks_and_run(void);
s32 world_shop_adjust_gil(s32 delta);
s32 world_shop_get_soldier_office_entry_fee(s32 unused_row);
void world_shop_return_unit_equip_to_fitting_room(s16 unit_index, s16 equipment_index);
void world_shop_run_equip_candidate_step(void);
s32 world_shop_run_fitting_room_checkout_prompt(void);
s32 world_shop_run_item_purchase_quantity_menu(void);
s32 world_shop_run_fur_buy_quantity_menu(void);
s32 world_shop_run_item_sell_menu(void);
s32 world_shop_run_fur_sell_quantity_menu(void);
void world_shop_run_obtain_gil_menu(void);
void world_shop_run_soldier_office_fee_menu_script(void);
void world_shop_update_unit_selection(void);
void world_shop_run_screen(s32 shop_type);

u32* world_shop_load_background_tim(s32 index);

/* other */
extern s16 g_world_field_object_use_request;
extern s16 g_world_debug_variable_list_request_state;
extern s32 g_world_preview_stats_window_active;
extern s8 g_world_dismiss_unit_prompt_running;
extern menu_entry_t g_world_best_fit_confirm_menu;
extern menu_entry_t g_world_order_unit_menu;
extern u8 g_world_best_fit_slot_categories[WORLD_EQUIPMENT_SLOT_COUNT];

/* Second slot-category table of world_shop_build_fitting_room_best_fit_equipment. */
extern u8 g_world_fitting_room_best_fit_slot_categories[WORLD_EQUIPMENT_SLOT_COUNT];
extern u8 g_world_order_unit_saved_browse_enabled;
s32 world_unit_get_facing_sixteenth_by_runtime_id(s32 runtime_id);
s32 world_coords_fit_in_byte(s32 x, s32 y);
void world_free_work_buffer(void);

/* unnamed */
extern s8 D_8018D18A;
extern s8 D_801C833C;
extern u8 D_801CC7D0;
extern u8 D_801CC7D8;
extern u8 D_801CD0EC;
extern u8 D_801CD0F4;

extern s16 g_world_formation_record_count; /* 0x801c8634 */
#endif
