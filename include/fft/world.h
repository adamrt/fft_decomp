#ifndef FFT_WORLD_H
#define FFT_WORLD_H

/* WORLD/WORLD.BIN: world map, menus and event scripts. */

#include "fft/main.h"
#include "psx/etc.h"
#include "psx/gs.h"

struct battle_ai_command_action;

/* camera */
/* Provisional camera interpolation key: frame time and target value. */
typedef struct world_camera_key {
    s32 time;  /* 0x00; cumulative frame of this key */
    s32 value; /* 0x04 */
    u8 _unused_08[8];
} world_camera_key_t;

/* Provisional per-component camera track, seeded by world_camera_fusion_thread
 * and advanced by world_camera_step_track. */
typedef struct world_camera_track {
    world_camera_key_t keys[8]; /* 0x00 */
    s32 key_count;              /* 0x80 */
    s32 segment;                /* 0x84; first key of the active segment */
    s32 frame;                  /* 0x88; frames since the track started */
    s32 segment_frame;          /* 0x8c; frames into the active segment */
    s32 prev_time;              /* 0x90; Q12 */
    s32 prev_value;             /* 0x94; Q12 */
    s32 start_value;            /* 0x98; Q12 */
    s32 segment_duration;       /* 0x9c */
    s32 finished;               /* 0xa0 */
} world_camera_track_t;

typedef char world_camera_track_offsets_ok[(sizeof(world_camera_key_t) == 0x10
                                               && (s32) & ((world_camera_track_t*)0)->key_count == 0x80
                                               && (s32) & ((world_camera_track_t*)0)->finished == 0xa0)
        ? 1
        : -1];

extern s32 g_world_camera_speed_curve;
extern u16 g_world_camera_script_variable_indices[7];
extern s16 g_world_camera_position_locked;
extern u16 g_world_camera_shake_z_offset;
extern s32 g_world_camera_yaw_band;
extern s32 g_world_camera_yaw_unwrap_request;
extern s16 g_world_camera_candidate_yaws[4];
s32 world_camera_wrap_yaw_angle(void);
void world_camera_init_tilt_and_zoom(void);
void world_camera_init_yaw_angle(void);
s32 world_camera_step_track(world_camera_track_t* track, s32 component);
void world_camera_store_yaw_band(s32 value);
s16 world_camera_pick_nearest_unblocked_yaw(s32 misc_id_a, s32 misc_id_b);
void world_camera_fusion_thread(void);
void world_camera_run_move_thread(void);
void world_camera_thread(void);

/* dead unit */
/* Provisional layouts for the dead-unit ("treasure/crystal") result panel
 * started by world_menu_start_dead_unit_threads. Only the fields that routine
 * touches are named; the surrounding bytes stay anonymous. */
typedef struct world_dead_unit_entry_counts {
    s16 visible_count;  /* 0x00 */
    s16 overflow_count; /* 0x02 */
} world_dead_unit_entry_counts_t;

typedef struct world_dead_unit_menu {
    /* 0x00-0x3b: the 0x3c-byte menu record (see world_menu_entry_t). */
    u8 _unused_000[4];
    s16 inner_width; /* 0x04 */
    u8 _unused_006[2];
    s16 window_x;     /* 0x08 */
    s16 window_y;     /* 0x0a */
    s16 window_width; /* 0x0c */
    u8 _unused_00e[6];
    s16 overall_width; /* 0x14 */
    u8 _unused_016[0x18];
    world_dead_unit_entry_counts_t* entries; /* 0x30 */
    u8 _unused_034[6];
    s16 menu_type;         /* 0x3a */
    u8 at_list_menu[0x78]; /* 0x3c */
    u8 confirm_menu[0x78]; /* 0xb4 */
    u8 message_menu[0x1c]; /* 0x12c */
    s16 message_id;        /* 0x148 */
    u8 _unused_14a[0x1e];
    u8 move_menu[4]; /* 0x168 */
} world_dead_unit_menu_t;

/* 0x00 selects which panel variant runs; 0x52 is the roster/item id. */
typedef struct world_dead_unit_context {
    s32 kind; /* 0x00 */
    u8 _unused_04[0x4e];
    u8 item_id;     /* 0x52 */
    u8 flags[0x40]; /* 0x53 */
} world_dead_unit_context_t;

extern s32 g_world_dead_unit_action; /* committed action */
extern s32 g_world_dead_unit_result;
extern world_dead_unit_entry_counts_t g_world_dead_unit_menu_entries;
extern world_dead_unit_menu_t g_world_dead_unit_menu;
extern RECT g_world_dead_unit_panel_b_texture_windows[7];

/* One libgpu texture window per DR_MODE packet (the seven RECTs after
 * g_world_dead_unit_panel_b_texture_windows). The static table holds each
 * window's w/h; world_menu_run_dead_unit_panel_a_2 fills in the runtime x/y.
 * Indexing the array reproduces the target's symbol-plus-byte-offset addressing. */
extern RECT g_world_dead_unit_panel_a_texture_windows[7];
extern u32* g_world_dead_unit_controller_input;
extern s16 g_world_dead_unit_menu_flags[256];
extern s16 g_world_dead_unit_menu_result; /* primary menu result */
extern s16 g_world_dead_unit_move_result; /* secondary (move) menu result */

/* help */
/* Help-menu descriptor for a pending help request. It is reached through the
 * native thread record's first function-parameter word, so its offsets are
 * its own and do not overlap native_thread_t. */
typedef struct world_help_request_table {
    u8 _unused_00[0x10];
    s16* primary_ids;   /* 0x10 */
    s16* secondary_ids; /* 0x14 */
    s16* mode_flag;     /* 0x18 */
} world_help_request_table_t;
typedef char world_help_request_table_mode_flag_offset_must_be_0x18
    [((unsigned long)&((world_help_request_table_t*)0)->mode_flag == 0x18) ? 1 : -1];

typedef struct world_help_request {
    u8 _unused_00[0x30];
    world_help_request_table_t* table; /* 0x30 */
    s16* value;                        /* 0x34 */
    s16 selected_index;                /* 0x38 */
    s16 kind;                          /* 0x3a */
} world_help_request_t;
typedef char
    world_help_request_kind_offset_must_be_0x3a[((unsigned long)&((world_help_request_t*)0)->kind == 0x3a) ? 1 : -1];

/* One help-menu cursor node: the value it edits, the four neighbour indices
 * and the cursor placement (tables at 0x801573ac, 0x8015743c, 0x801574ac and
 * 0x801574fc). */
typedef struct world_help_navigation_entry {
    s16* value;
    s16 value_base;
    u8 neighbors[4];
    s16 cursor_x;
    s16 cursor_y;
    s16 orientation;
} world_help_navigation_entry_t;
typedef char world_help_navigation_entry_size_must_be_0x10[sizeof(world_help_navigation_entry_t) == 0x10 ? 1 : -1];

extern u16 g_world_help_portrait_clut_by_sheet_id[];
extern u8 g_world_help_menu_cursor_tile[];
extern u8 g_world_help_active_banner[];
extern u8 g_world_help_active_unit_data[];

/* Copy of the 0x40-byte unit editor fields (world_gfx_copy_screen_setup_out);
 * the layout is battle_menu_status_panel_slot_state_t, as in the HELPMENU twin. */
extern battle_menu_status_panel_slot_state_t g_world_help_billboard;
extern world_help_navigation_entry_t g_world_help_terrain_navigation[];
extern world_help_navigation_entry_t g_world_help_attack_preview_navigation[];
extern world_help_navigation_entry_t g_world_help_unit_box_navigation[];
extern world_help_navigation_entry_t g_world_help_require_navigation[];
extern u8 g_world_help_menu_text_id_tables[];

/* Help-file offsets of the text sections the help menus rebind, added to
 * g_world_help_data_base (HELPMENU twin at 0x801e0b30). */
extern s32 g_world_help_text_section_offsets[32];
extern u8 g_world_help_data_base[];
extern s32* g_world_help_controller_state_ptr;
extern u16* g_world_help_topic_text_ids;
extern s32 g_world_help_text_id_tables_ptr;

/* map */
extern u16 g_world_map_append_state;
extern u16 g_world_map_reload_state;
extern map_selected_tile_t g_world_map_selected_tile_data;
extern s16 g_world_map_destruction_wait_state;
extern u16 g_world_map_refresh_pending;
extern u8 g_world_tile_info_image_location[];
extern world_gfx_image_load_parameters_t g_world_tile_info_image_params[2];
extern u16 g_world_map_darkness_wait_frames[];
extern s16 g_world_map_root_request_state;
extern s32 g_world_map_light_scale[4]; /* current light scale (VECTOR) */
extern MATRIX g_world_map_light_base_matrix;
extern s16 g_world_map_light_rotation[4]; /* current light rotation (SVECTOR) */
extern MATRIX g_world_map_light_color_matrix;
void world_map_data_load_complete_thread(void);
void world_map_draw_selected_tile_info_thread(void);
s32 world_map_is_busy(void);
void world_map_store_selected_tile_data(void);
void world_map_wait_for_refresh(void);
void world_map_light_thread(void);

/* name */
extern u8 g_world_name_entry_character_rows[];
extern u8 g_world_name_entry_display_script[];
extern u8 g_world_name_entry_keyboard_script[];
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
extern u16 g_world_name_entry_text_row_ids[];
extern world_menu_entry_t g_world_name_confirm_menu;
extern u8 g_world_name_confirm_initialized;
extern s8 g_world_name_keyboard_last_state; /* last drawn state */
extern s8 g_world_name_entry_last_state;    /* last drawn name-entry state */
extern u8 g_world_name_entry_input_delay;
extern u8 g_world_name_confirm_finished;
extern u8 g_world_name_used_original_name;
extern s16 g_world_name_entry_key_ids[];
extern u8 g_world_name_entry_state;
extern u8 g_world_name_entry_saved_state;
extern u8 g_world_name_entry_cursor_index;
extern u8 g_world_name_entry_list_cursor;
extern u8 g_world_name_entry_cursor;
extern u8 g_world_name_entry_text[17];
extern u8* g_world_name_entry_original_name;
extern s16 g_world_name_confirm_result;
void world_name_apply_entry_key(s16 key);
void world_name_run_confirmation_menu(void);
u8* world_name_run_entry_screen(u8* list, u32* image);
u8* world_name_run_entry_keyboard_frame(u8* list);
void world_name_update_entry_cursor(void);

/* options */
extern world_input_timing_profile_t g_world_option_input_timing_profiles[];
extern world_menu_text_binding_t g_world_option_menu_entry_table;
extern s32 g_world_option_menu_text_widths[];    /* option menu text-image width, per option */
extern s32 g_world_option_menu_entry_counts[];   /* option menu entry count, per option */
extern s32 g_world_option_menu_first_text_ids[]; /* first text id of the option's entries */
extern world_menu_icon_thread_param_t g_world_option_menu_icon_thread_param;
extern RECT g_world_options_menu_window_rect;
extern RECT g_world_options_menu_title_rect;
extern RECT g_world_options_menu_value_rect;
extern world_menu_icon_record_t g_world_option_picker_icon_records[2];
void world_build_options_menu(void);
void world_run_option_choice_menu(void);

/* screen */
/* Full-screen tint tile and its blend mode, one pair per packet buffer at
 * 0x801a6650, drawn by world_gfx_draw_screen_tint_tile. */
typedef struct world_screen_tint_prims {
    TILE tile;    /* 0x00 */
    DR_MODE mode; /* 0x10 */
} world_screen_tint_prims_t;

extern s32 g_world_screen_fade;
extern RECT g_world_screen_clear_rect;
extern world_screen_tint_prims_t g_world_screen_tint_tiles[]; /* one tint tile per packet buffer */

/* script */
/* Operand pairs that one world_script_execute_event opcode stores together;
 * each second halfword sits off a word boundary in WORLD bss, so each pair is
 * one object. */
typedef struct world_event_command_args {
    s16 arg1;
    s16 arg2;
} world_event_command_args_t;

typedef struct world_event_music_switch {
    s16 volume;
    s16 fade;
} world_event_music_switch_t;

/* WORLD stores ordinary variables as whole words, then packs the remaining
 * script-variable domains as 32 one-bit values or eight four-bit values per
 * word. */
enum {
    EVENT_SCRIPT_VAR_PACKED_BIT_FIRST = 0x80,
    EVENT_SCRIPT_VAR_PACKED_NIBBLE_FIRST = 0x360,
    EVENT_SCRIPT_VAR_END = 0x400,
    EVENT_SCRIPT_PACKED_BIT_WORD_FIRST = 0x80,
    EVENT_SCRIPT_PACKED_NIBBLE_WORD_FIRST = 0x97,
};

extern u32 g_world_event_current_input;
extern u32 g_world_event_initial_repeat_counter;
extern u32 g_world_event_previous_input;
extern u32 g_world_event_repeat_counter;
extern u32 g_world_event_secondary_repeat_counter;
extern s32 g_world_event_speed;
extern s32 g_world_script_acting_unit_id;
extern s32 g_world_script_loaded_event_id;
extern u32 g_world_script_tutorial_button_input;
extern s32 g_world_script_tutorial_command_active;
extern u8 g_world_script_tutorial_id;
extern u16 g_world_script_tutorial_shift_command_bit;
extern s32 g_world_script_tutorial_thread_switch_counter;
extern s16 g_world_script_tutorial_wait_time_remaining;
extern s16 g_world_script_tutorial_wait_timer;
extern s32* g_world_script_variables;
extern char g_world_script_pause_message[];
extern s32* g_world_event_words;
extern u16 g_world_script_attack_entry_mode;
extern s16 g_world_script_debug_paused;
extern s16 g_world_event_effect_target_x;
extern s16 g_world_event_effect_target_y;
extern u8 g_world_scenario_condition_arg_counts[]; /* instruction length by opcode */
extern u16 g_world_script_tutorial_latched_button;
extern u8 g_world_script_tutorial_activation_timer;
extern s16 g_world_script_tutorial_buttons;
extern s32 g_world_script_saved_event_input;
extern s16 g_world_event_map_jump_in_request;
extern s16 g_world_event_finish_check_pending;
extern u8 g_world_event_thread_status_snapshot[]; /* per-thread running flags sampled before the yield, indexed 1..15 */
extern world_unit_coordinates_t g_world_script_warp_unit_coords;
extern u32 g_world_event_random_seed;
extern volatile u32 g_world_script_tutorial_repeat_counters[16];
extern u16 g_world_script_tutorial_previous_input;
extern u8* g_world_script_tutorial_command_ptr;
extern void* g_world_event_pending_loaded_evtchr_slot;
extern void* g_world_event_loaded_evtchr_buffer;
extern s16 g_world_event_effect_target_misc_id;
extern s16 g_world_event_effect_target_mode;
extern u8 g_world_event_instruction_sizes[EVENT_OPCODE_COUNT];
extern world_event_command_args_t g_world_event_map_command_80_args;
extern world_event_command_args_t g_world_event_map_command_83_args;
extern world_event_music_switch_t g_world_event_music_switch;
extern s16 g_world_event_pending_effect_id;
extern s32 g_world_event_pending_evtchr_save_slot;
extern s32 g_world_event_pending_loaded_evtchr_clear_slot;
extern s16 g_world_event_pending_map_jump_out_2_id;
extern s16 g_world_event_pending_map_jump_out_id;
extern s16 g_world_event_pending_map_state;
extern s32 g_world_event_pending_reserved_vram_release_slot;
extern s32 g_world_event_pending_saved_evtchr_clear_slot;
extern s32 g_world_event_pending_unit_vram_copy;
extern const u8* g_world_event_script;
extern s16 g_world_event_unit_slots[4];
extern u8 g_world_event_variable_write_guard; /* WORLD counterpart: 0x80153387 */
extern s16 g_world_event_weather_request;
extern map_background_gradient_colors_t g_world_event_background_colors;
s32 world_script_check_scenario_condition(s32 a, s32 b, s32 c, s32 d, s32 e);
void world_script_handle_tutorial_command_highlight_ring(void);
s32 world_process_scenario_conditionals(void);
void world_script_run_sprite_move(void*, s32);
void world_script_set_specialized_map_destroyed(void);
void world_init_scene_bindings(void);
s32 world_script_filter_unit_id_by_mode(u16* out_id, u16* in_id, s32* mode);
u8* world_script_advance_cursor_with_repeat(s32* count, u8* cursor, u8** saved);
void world_script_advance_date_by_one_year(void);
void world_script_advance_tutorial_highlight_brightness(void);
void world_script_blue_remove_all_enemy_units(void);
void world_script_blue_remove_unit(s32 unit_id);
void world_script_change_stats(const u8* parameters);
void world_script_clear_current_event_word_bit_0(void);
void world_script_color_units(const u8* parameters);
void world_script_copy_32_bytes(void* destination, const void* source);
void world_script_copy_bytes(void* destination, const void* source, s32 count);
void world_script_earthquake_start(void);
void world_script_toggle_message_portrait_flip(const u8* parameters);
void world_script_apply_relative_camera(u8* source, const s32* deltas);
void world_script_face_tile(const u8* parameters);
void world_script_face_unit(const u8* parameters, s32 second_only);
void world_script_focus(u8* parameters);
void world_script_focus_speed(u8* parameters, s32* position, s32* rotation);
u16 world_script_get_current_scenario_finish_operation(void);

/* Event variable read via the 0xb1 instruction; 0x22 is rebuilt from 0x23/0x24. */
s32 world_script_get_variable(s32 variable_id);
s32 world_script_handle_tutorial_command(void);
void world_script_handle_tutorial_command_change_dialog(void);
void world_script_handle_tutorial_command_display_message(void);
s32 world_script_handle_tutorial_command_end(void);
void world_script_handle_tutorial_command_shift(void);
void world_script_handle_tutorial_command_wait(void);
void world_script_handle_tutorial_command_wait_for_button(void);
void world_script_handle_tutorial_command_wait_for_message(void);
void world_script_handle_tutorial_command_wait_time(void);
s32 world_script_interpolate_range_fixed12(s32 start, s32 end, s32 fraction, s32 value);
void world_script_load_attack_graphics_event_instruction(void);
void world_script_load_event(s32 id);
s32 world_script_load_next_event(void);
void world_script_map_darkness(void);
void world_script_march_units(const u8* parameters);
void world_script_play_effect_and_wait(void);
void world_script_print_debug_message(void);
void world_script_pulse_tutorial_wait_value(s32 value);
void world_script_rotate_unit_animation(const u8* parameters);
s32 world_script_run_frame(u32* ot, u32 buttons);
void world_script_run_next_event_2(void);
void world_script_seed_event_rng_from_vsync(void);
void world_script_set_event_speed(s32 speed);
void world_script_set_variable(s32 variable_id, s32 value);
void world_script_set_vsync_mode_and_event_speed(s32 value);
void world_script_show_graphic(void);
void world_script_sprite_move(void);
void world_script_sprite_move_beta(void);
void world_script_start_event_from_variables(void);
void world_script_start_tutorial(s32 index);
void world_script_store_halfword(u8* destination, u16 value);
void world_script_update_event_frame_input(u32* otag, u32 input, s16 frame_arg);
void world_script_switch_tutorial_thread(void);
void world_script_teleport_unit_out(s32 unit_id, s32 remove_unit);
void world_script_unit_anim(const u8* parameters);
void world_script_update_tutorial_controller_input(void);
void world_script_wait_for_unit_ready(s32 misc_id);
void world_script_wait_for_value(const u8* parameters);
void world_script_walkto_event_instruction(const struct event_walk_to_parameters* parameters);
s32 world_script_warp_unit(void*);
s32 world_script_warp_unit_display_to_paired_unit(s32);
void world_script_set_units_movement_effect_suppression(const u8* parameters);
void world_script_set_units_palette_update_suppression(u16 id, s32 enable);
s32 world_get_script_variable_bit_position(s32 variable_id);
void world_script_add_unit_start_thread(void);
s32 world_script_check_tutorial_event_slot(void);
void world_script_color_screen_thread(void);
void world_script_dismiss_unit_event_instruction(s32);
void world_script_execute_display_conditions_instruction(void);
void world_script_execute_event(void);
s32 world_script_find_instruction_byte_offset(s32 offset, s32 instruction);
s32 world_script_find_jump_target(s32, s32, s32, s32);
u32 world_script_get_random_u16(void);
s32* world_script_get_variable_word(s32 variable_id);
void world_script_inflict_status_thread(void);
s16 world_script_load_halfword(const u8* source);
void world_script_load_portrait_colors_event_instruction(s32);
void world_script_mirrorsprite_event_instruction(const u8* parameters);
void world_script_pause_event_instruction(void);
void world_script_play_effect_thread(void);
void world_script_run_condition(event_opcode_e opcode);
void world_script_run_variable_command(s32 opcode, s32 destination_id, s32 source, s32 unused);
void world_script_teleport_unit_in(s32 unit_id, s32 unused);
void world_script_unlockdate_event_instruction(u32* date_bits, s32 index, s32 month, s32 day);
void world_script_waitrotateunit_and_waitrotateall_event_instruction(s32);
void world_script_waitspritemove_event_instruction(s32);
void world_script_waitwalk_event_instruction(s32 unit_id);

/* sound */
extern s16 g_world_sound_current_music_track_index;
extern s32 g_world_sound_effect_id_to_play;
extern s16 g_world_sound_music_switch_request;
extern s16 g_world_sound_music_track_1_id;
extern s16 g_world_sound_music_track_2_id;
extern s16 g_world_sound_music_unload_slot_request;
extern s32 g_world_sound_music_volume_transition_request;
extern u8 g_world_sound_release_held_loop;
void world_sound_dispatch_effect(void);
void world_sound_play_music_tracks(s32 scenario_music, s32 scenario_tune);
void world_sound_set_effect_to_cancel(void);
void world_sound_set_effect_to_confirm(void);
void world_sound_set_effect_to_confirm_checked(void);
void world_sound_set_effect_to_invalid(void);
void world_sound_set_scroll_effect(void);
void world_sound_wait_for_music_stub(void);
void world_sound_bg_thread(void);
void world_sound_edit_bg_thread(void);

/* status */
/* Per-frame status panel primitive block (0x3d8 bytes), double buffered at
 * 0x801c3d8c and 0x801c453c. Same layout as attack_status_primitives_t in the
 * ATTACK twin. */
typedef struct world_status_frame {
    DR_MODE draw_modes[3];                                  /* 0x000 */
    SPRT sprites[24];                                       /* 0x024 */
    TILE tiles[2];                                          /* 0x204 */
    LINE_F2 lines[8];                                       /* 0x224 */
    battle_menu_status_panel_draw_offset_t draw_offsets[2]; /* 0x2a4 */
    world_menu_palette_primitives_t menu;                   /* 0x2c4 */
    world_gfx_scaled_draw_area_pair_t draw_area;            /* 0x3b0 */
} world_status_frame_t;
typedef char world_status_frame_size_must_be_0x3d8[sizeof(world_status_frame_t) == 0x3d8 ? 1 : -1];

/* Thread record of the task-0x3b status panels. The origin is read as whole
 * words (lw) for the draw-area offset; the halfword reads that feed the s16
 * draw-offset stores are the compiler narrowing those loads. */
typedef struct world_status_thread {
    s32 x;              /* 0x00 */
    s32 y;              /* 0x04 */
    s32 flags;          /* 0x08 */
    s32 redraw_request; /* 0x0c */
    s32 style;          /* 0x10 */
} world_status_thread_t;

/* Status-ID-indexed image source rectangles at 0x8018cf68. Not const: a const
 * qualifier lets GCC hoist world_item_build_status_list_polygons' loads above
 * its rect stores, which the target does not do. */
extern RECT g_world_status_display_image_rects[BATTLE_STATUS_COUNT];

/* Per-byte status removal masks applied while an event stages a unit. */
extern u8 g_world_status_removal_mask_unstaged[BATTLE_STATUS_BYTE_COUNT];
extern u8 g_world_status_removal_mask_staged[BATTLE_STATUS_BYTE_COUNT];
extern u8 g_world_status_removal_mask_staged_monster[BATTLE_STATUS_BYTE_COUNT];

/* Thread parameter blocks of the formation status panels (0x8018ba2c, eight
 * 0x14-byte world_status_thread_t records). */
extern world_status_thread_t g_world_status_display_thread_params;
extern world_status_thread_t g_world_preview_stats_thread_params;

/* status panel */
/* Menu primitive block written by the *_gfx_init_menu_tile_and_line_primitives
 * builders: three DR_MODE packets, two translucent 16x90 backdrop tiles and
 * eight vertical frame lines. The sprites between belong to
 * battle_menu_status_panel_buffer_t (battle.h) and are not touched by the
 * builder. */
typedef struct battle_menu_status_panel_menu_primitives {
    DR_MODE draw_modes[3]; /* 0x000 */
    u8 _unused_024[0x204 - 0x24];
    TILE tiles[2];    /* 0x204 */
    LINE_F2 lines[8]; /* 0x224 */
} battle_menu_status_panel_menu_primitives_t;
/* world_menu_init_column_frame_primitives (0x80110260) fills the shared menu
 * primitive block: three DR_MODEs select image pages 0/2/4; two 16x90 TILEs
 * and eight vertical LINE_F2 borders frame two columns. */
typedef char world_menu_column_frame_tiles_offset_must_be_0x204
    [((unsigned long)&((battle_menu_status_panel_menu_primitives_t*)0)->tiles == 0x204) ? 1 : -1];
typedef char battle_menu_status_panel_menu_primitives_size_must_be_0x2a4
    [(sizeof(battle_menu_status_panel_menu_primitives_t) == 0x2a4) ? 1 : -1];

/* Status panel sprite view used by the *_panel_set_primitive_colors helpers:
 * a 0x10-byte head followed by 0x14-byte SPRT packets, so sprites[n + 1] is
 * battle_menu_status_panel_buffer_t.sprites[n]. */
typedef struct battle_menu_status_panel_primitives {
    u8 header[0x10];
    SPRT sprites[25]; /* 0x010 */
} battle_menu_status_panel_primitives_t;
typedef char battle_menu_status_panel_primitives_size_must_be_0x204
    [(sizeof(battle_menu_status_panel_primitives_t) == 0x204) ? 1 : -1];

extern s16 g_world_auto_battle_option_colors[5]; /* text colour per option */
void world_run_battle_help_menu(void);

/* system */
/* Provisional: 0x14-byte system-function record (table at 0x80156750). */
typedef struct world_system_function {
    s16 value_00;               /* 0x00; copied to g_world_menu_current_id */
    u16 text_id;                /* 0x02; menu entry text id */
    s16 menu_entry_index;       /* 0x04; g_world_menu_thread_menu_data index */
    u16 value_06;               /* 0x06; copied to g_world_menu_system_function_row_actions */
    u16 value_08;               /* 0x08; menu entry field_0x20 */
    u8 _padding_0a[2];          /* aligns thread_entry */
    void (*thread_entry)(void); /* 0x0c; started as thread 8 */
    s16 alternate_id;           /* 0x10; replaces the id when navigation messages are not On */
    u8 _padding_12[2];          /* tail padding to 4-byte alignment */
} world_system_function_t;

/* System-function table. Declared as an array: as a scalar, GCC hoists
 * world_menu_run_system_function_thread's menu-index reload above its store. */
extern world_system_function_t g_world_system_function_table[];
extern s32 g_world_bin_load_request;
extern s32 g_world_primary_bit_read_index;
extern s32 g_world_primary_bit_write_index;
extern u8 g_world_primary_bit_cursor_reset;
extern u8* g_world_primary_bit_cursor;
extern s32 g_world_pending_bin_load_index;
extern s32 g_world_pending_bin_load_destination;
extern s32 g_world_system_function_thread_busy;
extern s32 g_world_bin_load_sectors[];         /* 0x80156388, indexed by request */
extern s32 g_world_bin_load_sizes[];           /* 0x801563c8 */
extern s32 g_world_bin_load_address_offsets[]; /* 0x80156408, relative to the overlay load address */
extern s32 g_world_pending_bin_load_sectors[];
extern s32 g_world_pending_bin_load_sizes[];
extern s32 g_world_pending_bin_load_in_progress;
extern s32 g_world_bin_load_in_progress; /* 0x801564cc */
extern s32 g_world_game_option_original_values[14];
extern s32 g_world_game_option_values[14];
void world_bin_load_file(s32 request);
s32 world_bit_cursor_read_primary(s32 bit_count);
s32 world_bit_cursor_read_secondary(s32 bit_count);
void world_bit_cursor_set_primary(u8* ptr);
void world_bit_cursor_set_secondary(u8* ptr);
void world_bit_cursor_write_primary(u8 bit);
void world_clamp_s32(s32* value, s32 min, s32 max);
void world_copy_bytes_to_s16_array(s16* destination, const u8* source, s32 count);
s32 world_mul_div_64(s32 a, s32 b, s32 c);
void world_game_reset(void);
void world_noop_800e2390(void);
void world_noop_800e90ec(void);
void world_noop_800ef9d4(void);
void world_noop_800f079c(void);
void world_noop_800f1388(void);
void world_noop_800f29d8(void);
void world_noop_800f6e98(void);
void world_noop_800f6ea0(void);
void world_noop_800f6ea8(void);
void world_noop_800f6f18(void);
void world_noop_800fdce8(void);
void world_noop_801325d4(void);
void world_process_bin_load_request(void);
void world_update_pending_file_load(void);
void world_noop_800e7808(s32 unused_unit_id);
void world_noop_800fd074(const u8* unused_parameters);
void printf(const char* fmt, ...);

/* thread */
/* Crystal/treasure pickup result (g_main_crystal_pickup_result, 0x8006623c)
 * returned through g_world_thread_inner_subroutine_result by
 * battle_unit_generate_crystal_or_treasure. Same object as the BATTLE
 * crystal_pickup_result_t view. */
typedef struct world_crystal_pickup_result {
    s32 result;          /* 0x00; -1 none, 4 treasure, 2 crystal, |1 learned */
    u8 _unused_04[0x14]; /* 0x04 */
    u8 learned[19][3];   /* 0x18 */
    u8 unit_index;       /* 0x51 */
    u8 treasure_item;    /* 0x52 */
} world_crystal_pickup_result_t;

extern u16 g_world_thread_change_cooldown;

/* Inner-subroutine continuation invoked by the WORLD thread scheduler. Five call
 * sites agree on this shape; world_script_teleport_unit_in reuses the slot with an
 * argument and keeps a local extern for that. */
extern void (*g_world_thread_inner_subroutine_callback)(void);
extern u8 g_world_thread_status_snapshot[16];

/* Nonzero while a modal task owns the menus (0x8015330c): the announce and
 * message threads and event opcode LOCK_MENU_INPUT set it, and
 * world_thread_update_task_state clears it once thread 1 stops. Menus drop
 * input and dim their palette while it is set. */
extern s32 g_world_thread_task_active;
extern s32 g_world_thread_task_ids[][256];
extern battle_menu_status_panel_indicator_prims_t g_world_thread_indicator_packets[2][2];
extern world_crystal_pickup_result_t* g_world_thread_inner_subroutine_result;

/* The unspecified argument list is intentional for the same stack-switching
 * callback bridge used by BATTLE. */
extern void (*g_world_thread_call_target)(void);
extern native_thread_t g_world_thread_contexts[];
extern s32 g_world_thread_current_id;

/* WORLD's pointer slot and fixed-array binding refer to the same scheduler
 * storage through different symbols. */
extern native_thread_t* g_world_threads;
s32 world_lookup_thread_parameter_threshold_value(s32 mode);
void world_thread_clear_current_slot(void);
s32 world_thread_find_running_at_or_after_4(void);
void world_thread_idle_wait_forever(void);
void world_thread_idle_wait_forever_b(void);
void world_thread_idle_yield_forever(void);
s32 world_thread_is_running_by_id(s32 thread_id);
s32 world_thread_is_task_active(void);
void world_thread_set_task_id_36(void);
void world_thread_update_task_state(void);
void world_thread_update_task_state_2(void);
void world_thread_wait_for_10_to_13(void);
void world_thread_yield_forever(void);
void world_thread_request_redraw(s32 thread_id);
s32 world_thread_call_on_main_stack();
void world_thread_exit_current(void);
s32 world_thread_find_running_by_task(s32 task_id);
void* world_thread_get_current_global_pointer(void);
void* world_thread_get_current_parameter_1(void);
s32 world_thread_get_current_parameter_2(void);
s32 world_thread_get_current_parameter_3(void);
s32 world_thread_get_current_task_id(void);
s32 world_thread_is_previous_running(void);
s32 world_thread_is_running(s32 thread_id);
s32 world_thread_is_running_80100164(s32 thread_id);
void world_thread_reset_scheduler(void);
s32 world_thread_resolve_id(s32 requested_thread_id);
s32 world_thread_resolve_id_after_current(s32 requested_thread_id);
void world_thread_resume(s32 thread_id);
void world_thread_set_current_task_id(s32 task_id);
void world_thread_set_parameters(s32 thread_id, s32 first, s32 second, s32 third);
void world_thread_set_parameters_4(s32 thread_id, s32 first, s32 second, s32 third, s32 fourth);
void world_thread_set_task_id_to_three(s32 thread_id);
void world_thread_start(s32 thread_id, void (*function)(void));
void world_thread_suspend(s32 thread_id);
void world_thread_wait_frames(s32 ticks);
void world_thread_wait_until_inactive(s32 thread_id);
void world_thread_yield(void);

/* turn */
/* One AT-list (turn order) descriptor. The four-byte stride is proven by the
 * index register world_menu_build_ability_preview_at_list advances by 4 and
 * by the +1 byte read for the turn value.
 *
 * flags bits: 0x1f = battle unit id, 0x1f meaning "end of list";
 *             0x20 = fixed "wait" label instead of a turn value;
 *             0x40 = the entry is the acting/queued unit (bracketed name);
 *             0x80 = turn value biased by 0x100. */
typedef struct world_at_descriptor {
    u8 flags;
    u8 turn_value;
    u8 _unused_02;
    u8 _unused_03;
} world_at_descriptor_t;

extern world_menu_list_page_t g_world_at_list_pages[2];
void world_build_at_list(void);
void world_build_at_list_2(void);

/* ability */
/* Final selector for the WORLD ability-list builder at 0x801228f0. */
typedef enum world_ability_list_mode {
    WORLD_ABILITY_LIST_MODE_VIEW_ALL = 0,
    WORLD_ABILITY_LIST_MODE_LEARNED_ONLY = 1,
    WORLD_ABILITY_LIST_MODE_LEARN_MENU = 2,
    WORLD_ABILITY_LIST_MODE_COUNT_UNLEARNED = 3,
} world_ability_list_mode_e;

/* Provisional ability-list window layout at 0x80153c78: a
 * world_menu_text_layout_t whose three columns are ids, values and extras
 * (x[0] is at 0x04). */
typedef struct world_ability_menu_layout {
    s16 visible_rows; /* 0x00 */
    s16 hidden_rows;  /* 0x02 */
    u8 _unused_04[2];
    s16 values_x;     /* 0x06: x[1] of world_menu_text_layout_t */
    s16 extras_x;     /* 0x08: x[2]; a column x origin, not a y */
    s16 ids_mode;     /* 0x0a: mode[0]; 0 text */
    s16 values_mode;  /* 0x0c: mode[1]; 3 alternate number or 2 hidden */
    s16 extras_mode;  /* 0x0e: mode[2] */
    u16* ids;         /* 0x10 */
    u16* values;      /* 0x14 */
    u16* extras;      /* 0x18 */
    s16* row_actions; /* 0x1c; indexed by the selected skill */
    u8 _unused_20[8];
    s16 row_offset; /* 0x28; first visible row */
} world_ability_menu_layout_t;
typedef char world_ability_menu_layout_size_must_be_0x2c[sizeof(world_ability_menu_layout_t) == 0x2c ? 1 : -1];

/* Partial menu position: 0x8011bc4c updates the y halfword at +2 and passes
 * the record to the numeric renderer at 0x801282dc. */
typedef struct world_menu_point {
    s16 x;
    s16 y;
} world_menu_point_t;

extern s16 g_world_ability_category;
extern s32 g_world_ability_selected_skill_target;
extern s32 g_world_ability_selected_type;
extern s32 g_world_secondary_bit_read_index;
extern u8 g_world_secondary_bit_cursor_reset;
extern u8* g_world_secondary_bit_cursor;
extern s32 g_world_spell_quote_last_ability_id;
extern world_ability_menu_layout_t g_world_ability_menu_layout;
extern u8 g_world_spell_quote_exception_skillsets[];
extern RECT g_world_ability_panel_origin;
extern RECT g_world_ability_panel_draw_area;
extern world_status_thread_t g_world_ability_panel_thread_params;
extern s8 g_world_ability_menu_initialized;
extern s8 g_world_remove_ability_initialized;
extern s8 g_world_set_ability_initialized;
extern s8 g_world_ability_view_list_initialized;
extern u8 g_world_ability_menu_slots_changed;
extern world_menu_entry_t g_world_ability_menu;
extern u8 g_world_ability_submenu_running;
extern u8 g_world_ability_menu_unit_index;
extern s8 g_world_remove_ability_unit_index;
extern u16 g_world_remove_ability_cursor_anim;
extern world_menu_point_t g_world_remove_ability_cursor_point;
extern world_menu_point_t g_world_clear_abilities_cursor_point;
extern u8 g_world_set_ability_window_script[];
extern s8 g_world_set_ability_slot;               /* ability slot under the cursor: 0..4 */
extern s8 g_world_set_ability_list_close_pending; /* close the ability list this frame */
extern s8 g_world_set_ability_show_stat_preview;  /* show the stat preview window */
extern u16 g_world_set_ability_cursor_anim;
extern world_menu_point_t g_world_set_ability_cursor_point;
extern u8 g_world_ability_list_script[];
extern s32 g_world_selected_ability;
extern world_menu_list_page_t g_world_ability_list_pages[][2];
extern u8 g_world_ability_menu_saved_browse_enabled;
extern s8 g_world_remove_ability_cursor;
extern s8 g_world_clear_abilities_prompt_active;
extern s16 g_world_remove_ability_slots[5];
extern volatile u16 g_world_clear_abilities_blink_counter;
extern s8 g_world_set_ability_list_open;   /* ability list open */
extern s8 g_world_set_ability_entry_count; /* ability list entry count */
extern u8 g_world_set_ability_unit_index;
extern u8 g_world_set_ability_cursor;

/* [0]: 0x0c = Learn Abilities, 0x0d = view only. */
extern s16 g_world_ability_entries[];
extern u8* g_world_selected_ability_secondary_data;
extern u8* g_world_selected_ability_data;
s32 world_ability_find_unit_abilities(s16 unit, s16 job, s32 kind, s16* out, s32 mode);
s32 world_ability_cache_entry_pointers(s32 index);

void world_ability_find_stat_changes_due_to_equipped(
    world_item_stat_detail_t* stat_diff, s16 old_ability, s16 new_ability);

s32 world_ability_get_ct_display_value(s32 index);
s32 world_ability_get_jp_cost_display_value(s32 index);
void world_ability_get_move_and_jump_increase_values(s16 ability, world_item_stat_detail_t* out);
s32 world_ability_get_mp_cost_display_value(s32 index);
s32 world_ability_has_secret_hunt(s16 unit_id);
s32 world_ability_has_two_hands(s16 unit_id);
s32 world_ability_has_two_swords(s16 unit_id);
s32 world_ability_is_category_action(void);
s32 world_ability_is_category_movement(void);
s32 world_ability_is_category_reaction(void);
s32 world_ability_is_category_support(void);
s32 world_ability_is_equip_support_change(s16 unit_id, s16 ability_id);
s32 world_ability_is_non_action_unlearned(s32 index);
s32 world_ability_is_selected_action_type(void);
void world_ability_learn(s16 unit_id, s16 job_id, s16 ability_id);
s32 world_ability_run_view_list(void);
s32 world_ability_build_equippable_rsm_list(s16 unit_index, s32 slot, s16* out);
s32 world_ability_run_learn_list(void);

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
extern world_gfx_image_load_parameters_t g_world_equipment_panel_sprite_params_0[19];
extern world_gfx_image_load_parameters_t g_world_equipment_panel_sprite_params_1[19];
extern world_gfx_image_load_parameters_t g_world_equipment_panel_sprite_params_2[19];
extern world_gfx_image_load_parameters_t g_world_equipment_panel_sprite_params_3[19];
extern world_gfx_image_load_parameters_t g_world_equipment_panel_icon_params[5];
extern world_status_thread_t g_world_equipment_ability_panel_thread_params;
extern u8 g_world_equip_menu_initialized;
extern u8 g_world_equip_item_menu_script[];
extern s8 g_world_equip_item_slot;
extern s8 g_world_equip_item_list_refresh;
extern s8 g_world_equip_item_preview_active;
extern u16 g_world_equip_item_cursor_anim;
extern world_menu_point_t g_world_equip_item_cursor_point;
extern u16 g_world_remove_all_equipment_ids[5];
extern world_menu_point_t g_world_remove_all_equipment_cursor_point;
extern s8 g_world_equip_item_list_open;
extern s16 g_world_equip_item_previewed_cursor;
extern s8 g_world_remove_all_equipment_prompt_active;
extern volatile u16 g_world_remove_all_equipment_blink_counter;

/* editor */
/* Positioned menu-number entry drawn by world_menu_draw_numeric_display_entries:
 * a screen position, a pointer to the live value, and the renderer selector.
 * The 0xC stride is proved by g_world_numeric_display_entries's three sublists at +0, +0x24 and
 * +0x54 (entries 0, 3 and 7). */
typedef struct world_menu_number_entry {
    u16 x;
    u16 y;
    s16* value;
    s16 param;
    s16 type;
} world_menu_number_entry_t;
typedef char world_menu_number_entry_size_must_be_0xc[(sizeof(world_menu_number_entry_t) == 0xC) ? 1 : -1];

extern u8 g_world_editor_numeric_geometry[];
extern RECT g_world_editor_numeric_table[2];
extern RECT g_world_editor_numeric_entries_a[];
extern RECT g_world_editor_numeric_entries_b[];
extern RECT g_world_editor_numeric_entries_c[];
extern RECT g_world_editor_numeric_entries_d[];
extern RECT g_world_editor_numeric_entries_e[];
extern RECT g_world_editor_numeric_entries_f[];
extern world_gfx_image_load_parameters_t g_world_editor_numeric_texture[];
extern world_menu_number_entry_t g_world_editor_numeric_descriptor_a[];
extern world_menu_number_entry_t g_world_editor_numeric_descriptor_b[];
extern battle_menu_status_panel_numeric_buffer_t g_world_editor_numeric_state_a[2];
extern battle_menu_status_panel_numeric_buffer_t g_world_editor_numeric_state_b[2];
extern u8 g_world_editor_numeric_text_a[];
extern u8 g_world_editor_numeric_text_b[];

/* comparison */
/* Display identity of a status-panel unit (0xe bytes; g_world_selected_unit_identity
 * and g_world_comparison_unit_identity). Formation code copies it from
 * world_formation_unit_t +0x22; wldcore_unit_build_status_panel_data fills it
 * from a party member. */
typedef struct world_unit_status_identity {
    s16 unit_index;  /* 0x00: name text 0x4000 + value (world_menu_unit_status_banner_thread) */
    s16 job_id;      /* 0x02: job name text 0x3000 + value */
    s16 brave;       /* 0x04 */
    s16 faith;       /* 0x06 */
    s16 zodiac;      /* 0x08 */
    s16 roster_slot; /* 0x0a: party slot (world_menu_get_party_unit_name_id); shown as value + 1 */
    u8 _unused_0c[2];
} world_unit_status_identity_t;
typedef char world_unit_status_identity_size_must_be_0xe[sizeof(world_unit_status_identity_t) == 0xe ? 1 : -1];

extern u8 g_world_comparison_unit_stat_summary[];
extern world_unit_status_identity_t g_world_comparison_unit_identity;
extern world_menu_number_entry_t g_world_comparison_unit_number_entries[];
extern RECT g_world_comparison_unit_portrait_rect;
extern world_status_thread_t g_world_comparison_display_thread_params;
extern u8 g_world_comparison_unit_summary_packets[];
extern u8 g_world_comparison_unit_status_packets[];
extern u8 g_world_comparison_unit_stat_text_image[];
extern u8 g_world_comparison_unit_name_text_image[];
extern u8 g_world_comparison_unit_number_text_image[];
extern u8 g_world_comparison_unit_portrait_image[];

/* gfx */
/* Separate decreasing/increasing RGB-intensity flags.
 * The getter packs them into bits 0/1 and returns 0 when disabled. */
typedef enum world_fade_state {
    WORLD_FADE_STATE_NONE = 0,
    WORLD_FADE_STATE_DECREASING = 1,
    WORLD_FADE_STATE_INCREASING = 2
} world_fade_state_e;

/* Provisional: record whose two TIM pointers at +0x1e00/+0x1e04 are filled
 * by world_gfx_load_tim_pair_by_index from the 8-byte LBA/size tables at
 * 0x80193c40 and 0x80193c78. Only these fields are known. */
typedef struct world_gfx_tim_pair_record {
    u8 _unused_0000[0x1e00];
    void* tim_pointers[2];
} world_gfx_tim_pair_record_t;
typedef char world_tim_pair_record_tims_offset_must_be_0x1e00
    [((unsigned long)&((world_gfx_tim_pair_record_t*)0)->tim_pointers == 0x1e00) ? 1 : -1];

/* Provisional 0x28-byte WORLD sprite record: a common 8-byte head followed by
 * two 16-byte halves that carry the left/right texture edges.  Adjusted by
 * the clip helpers at 0x8010cbdc / 0x8010cc40; field roles are inferred. */
typedef struct world_gfx_sprite_half {
    u16 x;          /* 0x00 */
    u16 y;          /* 0x02 */
    u8 u0;          /* 0x04 */
    u8 v0;          /* 0x05 */
    u16 clut;       /* 0x06 */
    u16 width;      /* 0x08 */
    u16 height;     /* 0x0a */
    u8 u1;          /* 0x0c */
    u8 v1;          /* 0x0d */
    u16 _unused_0e; /* 0x0e */
} world_gfx_sprite_half_t;

typedef struct world_gfx_sprite_record {
    u8 _unused_00[8];
    world_gfx_sprite_half_t halves[2]; /* 0x08, 0x18 */
} world_gfx_sprite_record_t;
typedef char world_sprite_record_size_must_be_0x28[(sizeof(world_gfx_sprite_record_t) == 0x28) ? 1 : -1];

/* Provisional: the WORLD GPU packet buffer pointed to by 0x801cd528.  Only the
 * ordering table base and the DR_MODE pool used by 0x8012d340 are named; the
 * 12-byte pool at 0x5c is consumed by the sibling 0x8012d25c; the TILE pool
 * at 0x3c by 0x8012bdbc. */
typedef struct world_gfx_packet_buffer {
    u32* otag; /* 0x00 */
    u8 _unused_04[0x0c - 0x04];
    POLY_F4* poly_f4s;        /* 0x0c; pool consumed by world_gfx_append_poly_f4_to_otag */
    POLY_FT4* textured_quads; /* 0x10; pool consumed by world_gfx_append_poly_ft4_to_otag */
    u8 _unused_14[0x1c - 0x14];
    POLY_G4* gradient_quads;           /* 0x1c; world_gfx_append_poly_g4_to_otag */
    POLY_GT4* textured_gradient_quads; /* 0x20; initialised by world_gfx_init_packet_pools */
    TILE* tiles_24;                    /* 0x24; pool consumed by world_menu_add_tile_primitive; the
                                          pool initialiser (0x80125e40) sets these up with SetLineF2 */
    u8 _unused_28[0x30 - 0x28];
    LINE_G2* gradient_lines; /* 0x30; world_menu_add_gradient_line_primitive */
    u8 _unused_34[0x3c - 0x34];
    TILE* tiles; /* 0x3c; pool consumed by 0x8012bdbc */
    u8 _unused_40[0x58 - 0x40];
    DR_MOVE* draw_moves; /* 0x58; pool consumed by world_gfx_add_draw_move_primitive */
    DR_AREA* draw_areas; /* 0x5c; pool consumed by world_gfx_add_draw_area_primitive */
    DR_MODE* draw_modes; /* 0x60 */
    DRAWENV draw_env;    /* 0x64; installed by world_gfx_present_frame_and_swap_packet_buffer */
    DISPENV disp_env;    /* 0xc0; installed by world_gfx_present_frame_and_swap_packet_buffer */
    u8 _unused_d4[0xec - 0xd4];
} world_gfx_packet_buffer_t;

typedef char world_packet_buffer_gradient_lines_offset
    [((unsigned long)&((world_gfx_packet_buffer_t*)0)->gradient_lines == 0x30) ? 1 : -1];

/* Provisional: sprite placement record passed to world_gfx_enqueue_oriented_textured_quad:
 * position and size followed by a world_formation_graphic_entry_t source rectangle
 * at 0x08 (world_menu_select_stat_gauge_level sets the halfword at 0x08). */
typedef struct world_gfx_sprite_desc {
    s16 x; /* 0x00 */
    s16 y; /* 0x02 */
    u8 _unused_04[0x0c - 0x04];
    s16 texture_width; /* 0x0c: source width; >= 0x19 selects the wide shadow offset */
    u8 _unused_0e[0x10 - 0x0e];
    u16 clut;  /* 0x10; set by world_formation_draw_unit_portrait_frame */
    u16 tpage; /* 0x12 */
} world_gfx_sprite_desc_t;

/* Provisional: 6-byte sprite slot (g_world_gfx_sprite_slots, 14 entries / 0x54 bytes, cleared by
 * world_gfx_clear_sprite_slots; written by world_gfx_set_sprite_slot; read by
 * world_gfx_get_sprite_slot). Matches the local world_gfx_sprite_slot_t view in
 * world_gfx_set_sprite_slot.c. */
typedef struct world_gfx_sprite_slot {
    s16 x;    /* 0x00 */
    s16 y;    /* 0x02 */
    s16 tile; /* 0x04; low 10 bits are the tile id */
} world_gfx_sprite_slot_t;

/* Primitive group submitted by world_gfx_submit_primitive_group: twelve GPU
 * primitives bracketed by a header and footer packet plus a trailing word.
 * Hoisted from the defining translation unit. */
typedef struct world_primitive_group {
    /* 0x00 */ s32 header[3];
    /* 0x0C */ s32 footer[3];
    /* 0x18 */ s32 prims[12][4];
    /* 0xD8 */ s32 tail;
} world_primitive_group_t;

/* Texture primitive placed by world_gfx_alloc_texture_grid_rect: a SPRT
 * (code 0x64-0x67) or a POLY_FT4. Both carry the CLUT at 0x0e, which the icon
 * slot helpers overwrite. */
typedef union world_texture_prim {
    SPRT sprt;
    POLY_FT4 ft4;
} world_texture_prim_t;

/* Textured menu quad source for world_gfx_append_poly_ft4_to_otag (0x8012ccd0):
 * four screen points, four texture points (only the low byte of each
 * halfword is used), then the CLUT and texture page. */
typedef struct world_gfx_textured_quad_source {
    s16 x0, y0;    /* 0x00 */
    s16 x1, y1;    /* 0x04 */
    s16 x2, y2;    /* 0x08 */
    s16 x3, y3;    /* 0x0c */
    u8 u0, u0_pad; /* 0x10 */
    u8 v0, v0_pad; /* 0x12 */
    u8 u1, u1_pad; /* 0x14 */
    u8 v1, v1_pad; /* 0x16 */
    u8 u2, u2_pad; /* 0x18 */
    u8 v2, v2_pad; /* 0x1a */
    u8 u3, u3_pad; /* 0x1c */
    u8 v3, v3_pad; /* 0x1e */
    u16 clut;      /* 0x20 */
    u16 tpage;     /* 0x22 */
} world_gfx_textured_quad_source_t;

/* Textured rectangle source for 0x8012cfd4: screen rectangle, texture
 * origin (only the low byte of u/v is used), CLUT and texture page. The
 * caller also passes four RGB triples, one per corner. */
typedef struct world_gfx_textured_rect_source {
    s16 x, y;         /* 0x00 */
    s16 w, h;         /* 0x04 */
    u16 u;            /* 0x08 */
    u16 v;            /* 0x0a */
    u16 clut;         /* 0x0c */
    u16 tpage;        /* 0x0e */
    u8 _unused_10[8]; /* 0x10; not read by 0x8012cfd4, but 0x80116e74 reserves it */
} world_gfx_textured_rect_source_t;
typedef char world_textured_rect_source_size_must_be_0x18[sizeof(world_gfx_textured_rect_source_t) == 0x18 ? 1 : -1];

/* Sprite placement record passed to world_gfx_enqueue_oriented_textured_quad:
 * screen rectangle, texture origin and size, CLUT and texture page. */
typedef struct world_oriented_quad {
    u16 x;
    u16 y;
    u16 w;
    u16 h;
    u8 u;
    u8 _unused_09;
    u8 v;
    u8 _unused_0b;
    u8 uw;
    u8 _unused_0d;
    u8 vh;
    u16 clut;
    u16 tpage;
} world_oriented_quad_t;

/* The same 0x14-byte sprite record with halfword texture fields, as the
 * formation cursor sprites, the item-category tab cursor and the job-wheel
 * mastered icon are written. */
typedef struct world_oriented_sprite {
    s16 x;     /* 0x00 */
    s16 y;     /* 0x02 */
    s16 w;     /* 0x04 */
    s16 h;     /* 0x06 */
    u16 u;     /* 0x08 */
    u16 v;     /* 0x0a */
    u16 u_w;   /* 0x0c */
    u16 v_h;   /* 0x0e */
    u16 clut;  /* 0x10 */
    u16 tpage; /* 0x12 */
} world_oriented_sprite_t;
typedef char world_oriented_sprite_size_must_be_0x14[sizeof(world_oriented_sprite_t) == 0x14 ? 1 : -1];

/* POLY_FT4 packet as the packed words libgs writes it. */
typedef struct world_poly_ft4_packet {
    u32 tag;       /* 0x00 */
    u32 mode;      /* 0x04: r | g << 8 | b << 16 | code << 24 */
    u32 xy0;       /* 0x08 */
    u32 uv0_clut;  /* 0x0c */
    u32 xy1;       /* 0x10 */
    u32 uv1_tpage; /* 0x14 */
    u32 xy2;       /* 0x18 */
    u32 uv2;       /* 0x1c */
    u32 xy3;       /* 0x20 */
    u32 uv3;       /* 0x24 */
} world_poly_ft4_packet_t;

/* The 0x10-byte glyph blit descriptor used by the dead-unit panels and filled
 * by world_gfx_set_image_blit_source_rect / _destination_origin. The stride is
 * the surface width in pixels. */
typedef struct world_glyph_blit {
    RECT rect;  /* 0x00 */
    s32 stride; /* 0x08 */
    u32 color;  /* 0x0c: colour word the decimal renderers set by value sign
                 * (0x77777777, 0xbbbbbbbb, ...) and clear after each draw */
} world_glyph_blit_t;
typedef char world_glyph_blit_size_must_be_0x10[sizeof(world_glyph_blit_t) == 0x10 ? 1 : -1];

extern world_status_frame_t g_world_change_banner_panel_frames[2];

/* Scenario interpreter interfaces shared by battle_script_execute_event and
 * world_script_execute_event. Signatures are as the interpreters bind them;
 * unverified names stay provisional. */
extern s16 g_world_3d_object_use_request;
extern u16 g_world_3d_object_wait_status;
extern u32* g_world_gfx_active_otag_entries;
extern world_gfx_packet_buffer_t* g_world_gfx_active_packet_buffer;
extern u16 g_world_gfx_draw_primitives_immediately;
extern u16 g_world_gfx_draw_suppress_flag;
extern u8 g_world_gfx_fade_decreasing;
extern u8 g_world_gfx_fade_disabled;
extern u8 g_world_gfx_fade_increasing;
extern s16 g_world_gfx_fade_intensity;
extern world_glyph_blit_t g_world_gfx_image_blit_destination;
extern world_glyph_blit_t g_world_gfx_image_blit_source;
extern world_gfx_sprite_slot_t g_world_gfx_sprite_slots[];
extern RECT g_world_gfx_temporary_draw_area;
extern u8 g_world_gfx_texture_allocation_grid[15][16];
extern s8 g_world_gfx_vsync_mode;
extern u16 g_world_gfx_textured_quad_count;
extern const char g_world_gfx_sort_sprite_overflow_message[];
extern world_image_location_t g_world_gfx_menu_image_source;
extern RECT g_world_gfx_texture_window;
extern u16 g_world_gfx_draw_area_scale_percent[];

/* A full-screen texture window (x=0, y=0, w=0x100, h=0x100 in the WORLD data
 * at 0x8016e44c), copied verbatim into a menu record's DR_MODE by
 * world_gfx_reset_record_texture_window_2; the formation view reads the same
 * bytes as a zero screen point. Same 8-byte RECT shape as
 * g_world_gfx_texture_window, whose reset sibling is byte-for-byte identical. */
extern const RECT g_world_gfx_full_texture_window;

/* Frame-buffer rectangles for the four WLDFACE pages; four separate objects,
 * since as RECT[4] world_gfx_load_wldface_to_frame_buffer shares one base. */
extern RECT g_world_wldface_vram_rects;
extern RECT g_world_wldface_vram_rect_1;
extern RECT g_world_wldface_vram_rect_2;
extern RECT g_world_wldface_vram_rect_3;
extern u16 g_world_portrait_clut_by_sheet_id[]; /* CLUT word per sprite-sheet id */
extern RECT g_world_gfx_portrait_origin;
extern const u16 g_world_gfx_panel_draw_area_scale_percent[12];
extern u8 g_world_gfx_portrait_transition_offsets[];
extern u8 g_world_gfx_black_tile_color[];
extern world_status_thread_t g_world_change_banner_panel_thread_params;
extern world_gfx_textured_quad_source_t g_world_vertical_cursor_quad;
extern RECT g_world_gfx_fade_tile_rect;
extern s32 g_world_gfx_tim_file_table[];
extern s32 g_world_gfx_tim_pair_primary_files[];   /* LBA/size pairs */
extern s32 g_world_gfx_tim_pair_secondary_files[]; /* LBA/size pairs */
extern TILE g_world_gs_clear_tile[];               /* per-buffer clear tile */
extern s32 g_world_gfx_free_rect_texels[4];
extern s32 g_world_gfx_free_rect_grid_cells[4];
extern RECT g_world_gfx_draw_area_template;
extern s16 g_world_gfx_draw_area_y;
extern u8 g_world_change_banner_text_image[];

/* Fade tile colour: the only byte-packed triple in WORLD bss; the fade
 * updaters pass &r as the rgb pointer and store g/b. */
extern CVECTOR g_world_fade_tile_color;
extern u16 g_world_gfx_tiles_24_capacity; /* +0x24 line count */
extern u16 g_world_gfx_line_f3_capacity;
extern u16 g_world_gfx_gradient_line_capacity; /* gradient line count */
extern u16 g_world_gfx_line_g3_capacity;
extern u16 g_world_gfx_line_f4_capacity;
extern u16 g_world_gfx_line_g4_capacity;
extern u16 g_world_gfx_poly_ft3_capacity;
extern u16 g_world_gfx_tiles_24_count;
extern u16 g_world_gfx_poly_gt3_capacity;
extern u16 g_world_gfx_textured_quad_capacity; /* textured quad count */
extern u16 g_world_gfx_line_f3_count;
extern u16 g_world_gfx_gradient_line_count;
extern u16 g_world_gfx_textured_gradient_quad_capacity; /* textured gradient quad count */
extern u16 g_world_gfx_line_g3_count;
extern u16 g_world_gfx_line_f4_count;
extern u16 g_world_gfx_line_g4_count;
extern u16 g_world_gfx_poly_ft3_count;
extern u16 g_world_gfx_poly_gt3_count;
extern u16 g_world_gfx_textured_gradient_quad_count;
extern u16 g_world_gfx_sprite_8_capacity;
extern u16 g_world_gfx_draw_buffer_clip_y;

/* Per-draw-buffer clip offsets: written by world_gs_set_draw_offsets, read by
 * world_gs_setdrawbuffclip. */
extern s16 g_world_gs_buffer_x[2]; /* display x per buffer */
extern s16 g_world_gs_buffer_y[2]; /* display y per buffer */
extern u16 g_world_gfx_sprite_16_capacity;
extern s16 g_world_gs_buffer_offset_x[2];
extern s16 g_world_gs_buffer_offset_y[2];
extern u16 g_world_ot_length;

/* Volatile: world_gfx_init_double_packet_buffers must read the draw-area count
 * after the draw-move count, as the target does. */
extern volatile u16 g_world_gfx_draw_area_capacity;
extern u16 g_world_gfx_draw_area_count;
extern u16 g_world_gfx_draw_mode_capacity;
extern u16 g_world_gfx_tile_capacity; /* tile count */
extern u16 g_world_gfx_draw_mode_count;
extern DVECTOR g_world_gs_ofs; /* libgs screen offset (the second halfword is not word aligned) */

/* Draw move count; volatile for the same read order as g_world_gfx_draw_area_capacity. */
extern volatile u16 g_world_gfx_draw_move_capacity;
extern u16 g_world_gfx_tile_count;
extern void* g_world_gs_out_packet_p;
extern u16 g_world_gfx_draw_move_count;
extern u16 g_world_gfx_sprite_count;
extern u16 g_world_gfx_tile_1_capacity;
extern u16 g_world_gfx_tile_8_capacity;
extern u16 g_world_gfx_tile_1_count;
extern u16 g_world_gfx_tile_8_count;
extern MATRIX g_world_gs_ls_matrix; /* GsLSMATRIX */
extern world_gfx_packet_buffer_t* g_world_gfx_packet_buffers;
extern u16 g_world_gfx_sprite_8_count;

/* The libgs draw environment (GsDRAWENV) shared by the world_gs_* routines. */
extern DRAWENV g_world_gs_drawenv; /* GsDRAWENV */
extern DISPENV g_world_gs_dispenv;
extern u16 g_world_gfx_tile_16_capacity;
extern s16 g_world_gs_offset_x;
extern s16 g_world_gs_offset_y;
extern u16 g_world_gfx_tile_16_count;
extern MATRIX g_world_gs_light_ws_matrix; /* GsLIGHTWSMATRIX */
extern u16 g_world_gfx_sprite_16_count;
extern s32 g_world_gs_screen_width;
extern s32 g_world_gs_screen_height;
extern MATRIX g_world_gs_id_matrix; /* GsIDMATRIX */
extern MATRIX g_world_gs_ws_matrix; /* GsWSMATRIX */

/* libgs clip rectangle: world_gs_reset_screen_state.c writes x/y/w/h here. */
extern RECT g_world_gs_clip_rect;
extern u16 g_world_gfx_poly_f3_capacity;
extern u16 g_world_gfx_poly_g3_capacity;
extern u16 g_world_gfx_poly_f4_capacity;
extern u16 g_world_gfx_gradient_quad_capacity; /* gradient quad count */
extern u16 g_world_gfx_poly_f3_count;
extern u16 g_world_gfx_poly_g3_count;
extern u16 g_world_gfx_poly_f4_count;
extern u16 g_world_gfx_gradient_quad_count;
extern s32 g_world_gs_frame_count; /* frame counter (never zero after wrap) */
extern s16 g_world_gs_active_buffer;
extern s16 g_world_gs_offset_draw_active;
extern u16 g_world_gfx_sprite_capacity;
extern s32 g_active_graphics_buffer_index;
void world_build_portrait_poly_ft4(s32 portrait_id, POLY_FT4* poly);
void world_display_specific_menu_text(s32 image, s32 origin, s32 text);
void world_display_menu_number_entry(s32 value, u8 digit_count, s32 image, u16* origin);

void world_gfx_build_scaled_draw_area_pair_at_offset(
    void* packets, const void* geometry, s32 buffer, s32 frame_offset, void* thread);

void world_gfx_add_draw_area_primitive(RECT* rect, s32 otag_index);
void world_gfx_add_draw_mode_primitive(s32 dfe, s32 dtd, s32 tpage, RECT* texture_window, s32 ot_index);
void world_gfx_add_draw_move_primitive(RECT* src, s32 x, s32 y, s32 ot_index);

void world_gfx_add_sprite_cell_quads(
    battle_menu_sprite_cell_t* cell, u8* rgb, s32 semi_trans, u16 tpage, u16 clut, s32 ot_index, s32 count);

void world_gfx_alloc_texture_grid_rect(RECT* rect, world_texture_prim_t* prim, u32* image);
void world_gfx_append_poly_g4_to_otag(s16* xy, u8* rgb, s32 semi_trans, s32 ot_index);
void world_gfx_append_poly_gt4_to_otag(world_gfx_textured_rect_source_t* source, u8* rgb, s32 semi_trans, s32 ot_index);
void world_gfx_append_tile_to_otag(RECT* box, u8* rgb, s32 semi_trans, s32 ot_index);
void world_gfx_bind_data_pointer(u32 index);
void world_gfx_build_scaled_draw_area_pair_swapped(void* packets, void* geometry, s32 step, s32 flag);
void world_gfx_clear_sprite_slot(s32 index);
void world_gfx_clear_sprite_slots(void);
void world_gfx_clear_texture_allocation_grid(void);

/* Portrait primitive helpers shared with the EVENT debug character editor.
 * world_gfx_clip_sprite_record_left/right are named for the way they move the
 * quad: _right shifts x right and trims the trailing texture edge (debugchr's
 * clip_portrait_poly_from_left), _left is debugchr's clip_..._from_right. */
void world_gfx_clip_sprite_record_left(world_gfx_sprite_record_t* record, s32 delta);
void world_gfx_clip_sprite_record_right(world_gfx_sprite_record_t* record, s32 delta);
void world_gfx_copy_screen_setup_in(const void* source_a, const void* source_b, const void* source_c);
void world_gfx_copy_screen_setup_out(void* dest0, void* dest1, void* dest2, void* dest3);
void world_gfx_draw_or_append_gpu_primitive(void* primitive);
void world_gfx_draw_screen_tint_tile(world_screen_tint_prims_t* prims, s32 intensity);
void world_gfx_draw_signed_number(s32 value, u8 digits, void* image, u16* x);

void world_gfx_enqueue_oriented_textured_quad(
    const world_oriented_quad_t* descriptor, const u8* color, s32 orientation, s32 semitrans, s32 otag_index);

void world_gfx_flip_poly_ft4_u_if_negative(POLY_FT4* poly, s32 direction);

void world_gfx_init_double_packet_buffers(world_gfx_packet_buffer_t* buffers, u32* otags, void* unused_2,
    void* unused_3, void* unused_4, POLY_FT4* textured_quads, void* unused_6, void* unused_7, POLY_G4* gradient_quads,
    POLY_GT4* textured_gradient_quads, TILE* tiles_24, void* unused_11, void* unused_12, LINE_G2* gradient_lines,
    void* unused_14, void* unused_15, TILE* tiles, void* unused_17, void* unused_18, void* unused_19, void* unused_20,
    void* unused_21, void* unused_22, DR_MOVE* draw_moves, DR_AREA* draw_areas, DR_MODE* draw_modes);

void world_gfx_init_image_loading(POLY_FT4* primitive, const world_image_location_t* base_load,
    const world_image_location_t* base_screen, const world_gfx_image_load_parameters_t* parameters);

void world_gfx_init_packet_pools(world_gfx_packet_buffer_t* buffer);
void world_gfx_lerp_poly_ft4_rect_b(POLY_FT4* poly, RECT* from, RECT* to, s32 t);
void world_gfx_load_tim_by_table_index(s32 index);
void world_gfx_mirror_4bpp_rect(u8* pixels, world_gfx_4bpp_rect_t* rect);
void world_gfx_move_image_and_wait(RECT* rect, s16 x, s16 y);
void world_gfx_reset_record_texture_window(void* record);
void world_gfx_reset_record_texture_window_3(void* record);
void world_gfx_set_blit_dest_position(s32 x, s32 y);
void world_gfx_set_fade_disabled(u8 disabled);
void world_gfx_set_image_draw_mode(DR_MODE* mode, s32 image_type);
void world_gfx_set_portrait_poly_texture(POLY_FT4* poly, s32 code);
void world_gfx_set_primitive_brightness(u8* primitive, s32 brightness);
void world_gfx_set_sprite_slot(s32 slot_index, s32 x, s32 y, u16* tile_table);
void world_gfx_submit_primitive_group(world_primitive_group_t* group);

void world_gfx_enqueue_textured_quad(
    RECT* rect, s32 u, s32 v, void* color, s32 semi_trans, s32 texture_page, s32 clut, s32 priority);

void world_gfx_free_texture_grid_rect(RECT* rect);
s32 world_gfx_get_fade_state(void);
void world_gfx_get_sprite_slot(s32 slot_index, s16* x, s16* y, s16* tile_table);
s32 world_gfx_get_vsync_mode_or_one(void);
void world_gfx_lerp_poly_ft4_rect(POLY_FT4* poly, RECT* from, RECT* to, s32 t);
void world_gfx_load_image_sync(RECT* rect, u32* data);
void world_gfx_load_tim_pair_by_index(s32 index, world_gfx_tim_pair_record_t* record);
void world_gfx_load_wldface_to_frame_buffer(void);
void world_gfx_move_image_to_ram_from_vram_and_wait(RECT* rect, u32* dest);
void world_gfx_present_frame_and_swap_packet_buffer(s32 first_otag, s32 draw_otag);
void world_gfx_ramp_screen_overlay_intensity_to_max(void);
void world_gfx_set_blit_source_rect(s16 a, s16 b, s16 c, s16 d);
void world_gfx_set_clut_rect_from_id(RECT* rect, s32 clut_id);
void world_gfx_set_draw_clip_rectangle(const volatile RECT* source);
void world_gfx_set_draw_mode_from_rect(DR_MODE* mode, const u16* rect);
void world_gfx_set_image_blit_destination_origin(s16 x, s16 y);
void world_gfx_set_image_blit_source_rect(s16 x, s16 y, s16 width, s16 height);
void world_gfx_start_decreasing_fade(void);
void world_gfx_start_increasing_fade(void);
void world_gs_sortpoly(POLY_FT4* poly, s32 arg, s32 type, u32 value);
s32 world_gfx_update_fade_in_tile(void);
s32 world_gfx_update_fade_out_tile(void);
void world_gs_clear_ot(s32, s32, void*);
void world_gs_draw_ot(void*);
s32 world_gs_get_active_buffer(void);
void world_gs_gte_init(void);
void world_gs_init3d(void);
void world_gs_initgraph(u16 x, u16 y, u16 intmode, u16 dither, u16 vram);
void world_gs_reset_screen_state(u16 width, u16 height);
void world_gs_set_draw_offsets(s32 x0, s32 y0, s32 x1, s32 y1);
void world_gs_setdrawbuffclip(void);
void world_gs_setdrawbuffoffset(void);
void world_set_bitmap_bit(u32* bits, s32 index, s32 set);
void world_gfx_load_evtchr_thread(void);
void world_gfx_refresh_script_unit_environment_palette(const u8* parameters);
void world_gs_sortboxfill(GsBOXF* box, void* table, s32 priority);
void world_gs_sortline(GsLINE* line, GsOT* ot, u16 pri);
void world_gs_gettiminfo(u32* tim, GsIMAGE* image);
void world_gs_set_display_mode(u16 x, u16 y, u16 intmode, u16 dither, u16 vram);
void world_gs_sortclear(u8 r, u8 g, u8 b, GsOT* otp);
void world_gs_sortsprite(GsSPRITE* sp, GsOT* ot, u16 pri);
void world_gs_swapdispbuff(void);
void world_gs_setworkbase(void* base);

/* job */
/* A job-wheel portrait quad (0x8018c8a4 and 0x8018c8b4): a
 * world_gfx_textured_rect_source_t without its unread trailing 8 bytes. */
typedef struct world_job_wheel_portrait_quad {
    s16 x, y;  /* 0x00 */
    s16 w, h;  /* 0x04 */
    u16 u;     /* 0x08 */
    u16 v;     /* 0x0a */
    u16 clut;  /* 0x0c */
    u16 tpage; /* 0x0e */
} world_job_wheel_portrait_quad_t;

extern s32 g_world_job_cached_level;
extern s32 g_world_job_cached_level_jp_requirement;
extern s32 g_world_job_cached_skillset_empty;
extern s32 g_world_job_cached_total_jp;
extern s16 g_world_job_wheel_job_count;
extern u16 g_world_job_wheel_name_text;
extern s8 g_world_learn_menu_initialized;
extern u8 g_world_job_wheel_initialized;
extern s8 g_world_learn_job_list_initialized;
extern u8 g_world_learn_ability_list_open;
extern s8 g_world_view_abilities_initialized;
extern s16 g_world_job_wheel_unit_index;

/* The current and the new job's portrait; separate objects, since
 * world_formation_run_job_wheel_menu addresses each with its own lui/addiu. */
extern world_job_wheel_portrait_quad_t g_world_job_wheel_portrait_quad;
extern world_job_wheel_portrait_quad_t g_world_job_wheel_new_portrait_quad;
extern u8 g_world_job_wheel_script[];
extern u8 g_world_job_wheel_job_selected;
extern world_menu_point_t g_world_job_wheel_unit_sprite_position;
extern u8 g_world_job_wheel_center_x;
extern u8 g_world_job_wheel_center_y;
extern u8 g_world_job_wheel_animation_counter;
extern RECT g_world_job_change_old_sprite_rect;
extern RECT g_world_job_change_new_sprite_rect;
extern u8 g_world_job_change_active;
extern s16 g_world_job_change_frame;
extern u8 g_world_job_change_wave_colors[];
extern u8 g_world_job_wheel_close_frame;
extern u8 g_world_job_wheel_open_frame;
extern u8 g_world_job_wheel_closing;
extern u8 g_world_job_wheel_opening;
extern world_oriented_sprite_t g_world_job_wheel_mastered_icon_sprite;
extern u8 g_world_learn_job_list_script[];
extern u8 g_world_learn_confirm_menu[];

/* The prompt chained after g_world_learn_confirm_menu. */
extern world_menu_icon_thread_param_t g_world_learn_confirm_submenu;
extern u16 g_world_learn_job_name_list[];
extern u16 g_world_view_job_name_list[];
extern s8 g_world_job_wheel_spin_progress;
extern s8 g_world_job_wheel_spin_direction;
extern s8 g_world_job_change_anim_state;
extern u8 g_world_job_change_confirmed;
extern u8 g_world_job_wheel_saved_browse_enabled;
extern u8 g_world_job_change_flash_colors[4][3];
extern s16* g_world_job_change_dissolve_order;
extern u8* g_world_job_change_old_sprite_image;
extern u8* g_world_job_change_new_sprite_image;
extern u8* g_world_job_change_new_sprite_source;
extern s16 g_world_job_change_skip_requested;
extern s16 g_world_job_wheel_cursor_index;
extern u16 g_world_job_wheel_jobs[];
extern s16 g_world_job_wheel_mastered_jobs[];
extern RECT g_world_job_wheel_name_rect;
extern s8 g_world_learn_menu_step; /* learn menu sub-state */

/* Nonzero while the learn-confirmation menu thread (thread 6) is running. */
extern u8 g_world_learn_confirm_prompt_state;
extern s8 g_world_view_abilities_step; /* menu sub-state */
extern u8 g_world_view_abilities_confirm_prompt_state;
extern u16 g_world_learn_selected_job;
extern s16 g_world_learn_selected_job_index;
s32 world_job_build_unit_list(u8 unit_index, s16* jobs, s32 mode);
s32 world_job_find_by_skillset(s32 skillset);

/* Job id -> generic job index 0..0x13, else 0. */
s16 world_job_get_generic_index(u16 job_id);
s32 world_job_get_skillset(s32 job_id);
s16 world_job_get_text_index(s16 job_id);
s32 world_job_is_special_monster(s32 job_id);

/* Menu script callbacks: only ever address-taken and cast to
 * menu_script_callback_t by world_ability_run_view_list. */
s16 world_job_get_base(s16 formation_idx);
s32 world_job_get_cached_level(void);
s32 world_job_get_cached_level_jp_requirement(void);
s32 world_job_get_cached_skillset_empty_flag(void);
s32 world_job_get_cached_total_jp(void);
u16 world_job_get_selected_unit_job_points_entry(void);
s32 world_job_refresh_next_level_jp_requirement(s32 index);
s32 world_job_is_locked_for_all_units(s16 job_id);

/* panel */
/* Provisional layout of the dead-unit panel primitive block at 0x801cc074,
 * which holds two identical frame records. The 0x158 record size is fixed by
 * the second frame starting there and by the byte copy between them. Members
 * come from the panels' own accesses plus two helpers:
 * world_menu_init_sprite_array (0x8012e2b8) is handed the nine SPRT packets,
 * and world_gfx_reset_record_texture_window (0x80138adc) writes +0x1c and the
 * 8-byte window at +0x20 of the cursor record, which lands on the cursor
 * sprite's w and the RECT that follows it. The block is zero-filled in the
 * overlay image, so none of it is initialized data. */
typedef struct world_panel_frame {
    DR_MODE modes[7];                  /* 0x000: one per texture window */
    SPRT sprites[9];                   /* 0x054: 5 border spans then 4 corners */
    world_texture_prim_t texture_prim; /* 0x108: tpage-7 VRAM allocation record */
    DR_MODE cursor_mode;               /* 0x130 */
    SPRT cursor;                       /* 0x13c */
    RECT cursor_texture_window;        /* 0x150 */
} world_panel_frame_t;
typedef char world_panel_frame_size_must_be_0x158[sizeof(world_panel_frame_t) == 0x158 ? 1 : -1];

/* Provisional 0x1ac-byte double-buffered panel record at 0x801a2bd0. */
typedef struct world_panel_record {
    DR_MODE mode0;                                /* 0x000 */
    DR_MODE mode1;                                /* 0x00c */
    world_gfx_scaled_draw_area_pair_t draw_areas; /* 0x018; world_gfx_reset_record_texture_window */
    SPRT sprites[18];                             /* 0x040 */
    world_menu_palette_primitives_t* palette;     /* 0x1a8 */
} world_panel_record_t;

extern s16 g_world_panel_slide_up_y[];
extern u8 g_world_panel_item_icon_texture[];
extern s16 g_world_panel_slide_down_y[];

/* tutorial */
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
    WORLD_TUTORIAL_CMD_WAIT_FOR_MESSAGE_2 = 0x16,
    WORLD_TUTORIAL_CMD_END = 0x17,
};

extern s16 g_world_tutorial_categories_request_state;
extern world_gfx_tim_pair_record_t g_world_tutorial_saved_game_image;
extern void* g_world_tutorial_text_buffer;
extern void* g_world_tutorial_script_buffer;
extern s16 g_world_tutorial_highlight_corners_a[][4]; /* first tutorial highlight quad corners */
extern s16 g_world_tutorial_highlight_corners_b[][4]; /* second tutorial highlight quad corners */
extern u8 g_world_tutorial_highlight_colors_a[4][3];  /* first quad corner colours */
extern u8 g_world_tutorial_highlight_colors_b[4][3];  /* second quad corner colours */
extern u8 g_world_tutorial_highlight_quad_count;

/* input */
typedef struct world_menu_input_state {
    s32 new_buttons;
    s32 current_buttons;
    s32 previous_buttons;
    s32 repeat_counter_0;
    s32 repeat_counter_1;
    s32 repeat_counter_2;
} world_menu_input_state_t;

extern u8 g_world_input_activation_timer;
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
extern u16 g_world_input_previous, g_world_input_latched_button, g_world_input_newly_pressed;
extern volatile u32 g_world_input_repeat_counters[16];
extern u32 g_world_input_primary_repeat, g_world_input_secondary_repeat;
extern u32 g_world_input_unfiltered_controller;
extern world_input_timing_profile_t g_world_input_timing_profiles[];
extern u8 g_world_input_triangle_latched;
extern u8 g_world_input_triangle_latch;
extern u8 g_world_input_blocked_state;
extern u32* g_world_input_frame_controller_input;
extern s32 g_world_input_primary_repeat_latched;
extern s32 g_world_input_secondary_repeat_latched;
void world_input_clear_state(void);
s32 world_input_filter_menu(void);
s32 world_input_get_lockout_timer(void);
u32* world_input_get_menu_controller(s32 player);
s32 world_input_get_menu_repeat_counter(void);
s16 world_input_get_tutorial_buttons(void);
s32 world_input_read_controller(s32 unused);
s32 world_input_read_menu_scroll_repeat(void);
s32 world_input_read_tutorial_or_controller(void);
void world_input_store_menu_state(const world_menu_input_state_t* state);
void world_input_update_controller(void);
void world_input_update_menu_repeat(u32 buttons);
void world_input_update_menu_state(void);

/* formation */
/* Provisional 12-byte formation portrait source rectangle.
 *
 * The builder replaces clut after copying a table entry. tpage selects the
 * VRAM x base: 0x100 for 0x64, else 0x140. */
typedef struct world_formation_graphic_entry {
    u16 x;     /* 0x00 */
    u16 y;     /* 0x02 */
    u16 w;     /* 0x04 */
    u16 h;     /* 0x06 */
    u16 clut;  /* 0x08 */
    s16 tpage; /* 0x0a: GetTPage(0, 3, 0x100 or 0x140, 0), 0x64 or 0x65 */
} world_formation_graphic_entry_t;
typedef char world_formation_graphic_entry_size_must_be_0xc[(sizeof(world_formation_graphic_entry_t) == 0xc) ? 1 : -1];

/* Provisional: formation view record at 0x8018a144 driven by the thread at
 * 0x80108f9c. Only the fields it touches are named. */
typedef struct world_formation_view {
    void* ordering_table; /* 0x00 */
    s32 color;            /* 0x04; copied from g_world_menu_color_red */
    s32 link_quads;       /* 0x08; nonzero: 0x80108ce8 links every quad into the ordering table */
    s16 scale_x;          /* 0x0c; 12-bit fixed, eased down to 0x1000 */
    s16 scale_y;          /* 0x0e */
    u8 _unused_10[8];
    u8 origin[8]; /* 0x18; start of the world_formation_sprite_origin_t view whose x/y are offset_x/offset_y */
    s16 offset_x; /* 0x20: added to every quad x (0 or 0x100 by screen mode) */
    s16 offset_y; /* 0x22: added to every quad y (8 or 0x80) */
} world_formation_view_t;

/* The stat table installed by 0x8011604c stores getters with these two
 * verified return widths. Selecting a getter must preserve its signature. */
typedef union world_formation_stat_callback {
    s32 (*wide)(s32 formation_index);
    s16 (*halfword)(s32 formation_index);
} world_formation_stat_callback_t;

/* Provisional: signed view of world_formation_graphic_entry_t as read by
 * world_formation_build_graphic_quad (0x80108a18), which multiplies w/h and
 * the 0x08/0x0a offsets by a 12-bit fixed scale as signed halfwords. */
typedef struct world_formation_graphic_rect {
    s16 u;     /* 0x00 */
    s16 v;     /* 0x02 */
    s16 w;     /* 0x04 */
    s16 h;     /* 0x06 */
    s16 x_off; /* 0x08 */
    s16 y_off; /* 0x0a */
} world_formation_graphic_rect_t;

/* Provisional: texture page location (0x80173e4c) and screen point
 * (0x80173d7c) passed to world_formation_build_graphic_quad. */
typedef struct world_formation_texture_page_location {
    s16 x; /* 0x00 */
    u16 y; /* 0x02: only bits 8-11 reach GetTPage */
} world_formation_texture_page_location_t;

typedef struct world_formation_screen_point {
    u16 x; /* 0x00 */
    u16 y; /* 0x02 */
} world_formation_screen_point_t;

/* 12-bit fixed-point scale pair. */
typedef struct world_formation_fixed_scale {
    s16 x; /* 0x00 */
    s16 y; /* 0x02 */
} world_formation_fixed_scale_t;

/* Provisional: the formation view record from +0x18 as seen by
 * world_formation_build_graphic_quad (its caller passes view + 0x18); the
 * two words at +0x08/+0x0a are the view's offset_x/offset_y origin. */
typedef struct world_formation_sprite_origin {
    u8 _unused_00[8];
    u16 x; /* 0x08 */
    u16 y; /* 0x0a */
} world_formation_sprite_origin_t;

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
    u8 month;
    u8 day;
    u8 zodiac;
} world_zodiac_date_limit_t;
typedef char world_zodiac_date_limit_size_must_be_3[(sizeof(world_zodiac_date_limit_t) == 3) ? 1 : -1];

/* Formation "order" menu entry; eight at 0x8018ba38 (see
 * world_menu_enable_all_order_entries). Entry 0's flag word also carries the
 * selection-slide direction. */
typedef struct world_order_menu_entry {
    s32 enabled; /* 0x00 */
    u8 _unused_04[0x10];
} world_order_menu_entry_t;

/* Provisional: cursor trail position history at 0x801c8344, reset by
 * world_formation_reset_slots_and_frame_tpages; entry 7 is the newest. */
typedef struct world_cursor_trail_point {
    s16 x; /* 0x00; -1 when unused */
    s16 y; /* 0x02 */
} world_cursor_trail_point_t;

enum {
    WORLD_ZODIAC_DATE_LIMIT_COUNT = 13,
    WORLD_MONTH_LENGTH_TABLE_COUNT = 13,
    WORLD_MONSTER_BREED_ENTRY_COUNT = JOB_ID_TIAMAT - JOB_ID_CHOCOBO + 1,
};

/*
 * WORLD menu unit record, compacted after roster and menu-specific filtering.
 * Records at 0x801c8638 have stride 0x128; roster_slot preserves their roster
 * identity. The builder at 0x801210e8 and sorter at 0x80121c60 produce the
 * ordered g_world_formation_unit_pointers list, not a roster-indexed array.
 * The byte fields from 0x70 on mirror the party_data_t layout above. Magical
 * shield/accessory evade are provisionally placed at 0x50/0x52, after the
 * physical fields at 0x40/0x42, to follow the halfword sequence. Other bytes
 * stay as padding.
 */
typedef struct world_formation_unit {
    s16 level;        /* 0x000 */
    s16 guest_marker; /* 0x002; 2 for guest slots */
    u8 _unused_004[4];
    s16 experience;  /* 0x008 */
    s16 party_index; /* 0x00a; portrait */
    s16 hp;          /* 0x00c */
    u8 _unused_00e[2];
    s16 max_hp; /* 0x010 */
    s16 mp;     /* 0x012 */
    u8 _unused_014[2];
    s16 max_mp; /* 0x016 */
    s16 ct;     /* 0x018 */
    u8 _unused_01a[2];
    s16 max_ct; /* 0x01c; always 100 (world_formation_build_unit_record) */
    u8 _unused_01e[4];
    u8 name_index[2]; /* 0x022; s16 formation index; starts the 14-byte display snapshot at WORLD 0x80114bc8 */
    s16 job_id;       /* 0x024; "Current Job" */
    s16 brave;        /* 0x026 */
    s16 faith;        /* 0x028 */
    s16 zodiac;       /* 0x02a; battle_stats_t birthday >> 12 (world_formation_build_unit_record) */
    s16 roster_slot;  /* 0x02c; "Roster Slot Index" */
    u8 _unused_02e[2];
    s16 move;                     /* 0x030 */
    s16 speed;                    /* 0x032 */
    s16 jump;                     /* 0x034 */
    s16 right_weapon_power;       /* 0x036 */
    s16 left_weapon_power;        /* 0x038 */
    s16 right_weapon_evade;       /* 0x03a */
    s16 left_weapon_evade;        /* 0x03c */
    s16 uses_monster_skillset;    /* 0x03e; primary skillset is in the monster range 0xb0..0xdf */
    s16 two_handed;               /* 0x040; world_item_check_two_hands_for_weapons result (WORLD 0x80115198) */
    s16 physical_attack;          /* 0x042 */
    s16 physical_class_evade;     /* 0x044 */
    s16 physical_shield_evade;    /* 0x046 */
    s16 physical_accessory_evade; /* 0x048 */
    u8 _unused_04a[2];
    s16 magical_attack;          /* 0x04c */
    s16 magical_class_evade;     /* 0x04e */
    s16 magical_shield_evade;    /* 0x050 */
    s16 magical_accessory_evade; /* 0x052 */
    u16 equipment[5];            /* 0x054; right hand, left hand, head, body, accessory */
    s16 primary_skillset;        /* 0x05e */
    s16 secondary_skillset;      /* 0x060 */
    s16 reaction_ability;        /* 0x062 */
    s16 support_ability;         /* 0x064 */
    s16 movement_ability;        /* 0x066 */
    u8 _unused_068[8];
    u8 gender_flags; /* 0x070; unit_flags_e (fft/unit.h) */
    u8 _unused_071;
    u8 sprite_set;               /* 0x072; "Sprite Set ID": party_data_t.sprite_set / character identity */
    u8 equippable_item_types[4]; /* 0x073; bit per item type, read by can_unit_equip_item_id */
    u8 unlocked_jobs[3];         /* 0x077 */
    u8 learned_abilities[0x39];  /* 0x07a through 0x0b2 */
    u8 job_levels[0x0a];         /* 0x0b3 */
    u8 _padding_0bd;             /* aligns job_points */
    u16 job_points[0x14];        /* 0x0be; per-job JP */
    u16 total_job_points[0x14];  /* 0x0e6; per-job total JP */
    u8 name[0x10];               /* 0x10e; "unit_t Nickname" */
    u8 graphic_variant;          /* 0x11e; added to formation graphic entry 0x3d when sprite_set == 0x82
                                    (world_formation_build_unit_graphic_entry) */
    u8 proposition_status;       /* 0x11f; "Proposition Byte" */
    u16 birthday;                /* 0x120; battle_stats_t birthday & 0x1ff; egg readers split
                                    it into nibbles */
    u16 egg_color; /* 0x122; party_data_t 0xd2; the formation sprite drawer (0x8011751c) loads it as a halfword palette
                      index */
    u8 support_abilities[4]; /* 0x124; copy of battle_stats_t.support_abilities, BATTLE_SUPPORT_SET_1..4_* */
} world_formation_unit_t;
typedef char world_formation_unit_t_size_must_be_0x128[(sizeof(world_formation_unit_t) == 0x128) ? 1 : -1];

/* Slot-indexed view of the five ability halfwords at 0x5e (primary skillset,
 * secondary skillset, reaction, support, movement).  The WORLD ability-slot
 * menu walks them by slot number; the bytes are the named fields above. */
typedef struct world_formation_unit_ability_slots {
    u8 _unused_00[0x5e];
    u16 ability_slots[5]; /* 0x05e */
} world_formation_unit_ability_slots_t;
typedef char world_formation_unit_ability_slots_offset_must_be_0x5e
    [((unsigned long)&((world_formation_unit_ability_slots_t*)0)->ability_slots == 0x5e) ? 1 : -1];

/* Menu-script "draw number" command: an opcode/length prefix followed by the
 * value getter, layout and byte position operands. */
typedef struct world_draw_number_command {
    u8 opcode;
    u8 length;
    u8 style;
    u8 getter;
    u8 width;
    u8 spacing;
    u8 alternate_palette;
    u8 x;
    u8 y;
    u8 value;
} world_draw_number_command_t;

extern world_oriented_sprite_t g_world_formation_cursor_shadow_sprite;
extern world_oriented_sprite_t g_world_formation_cursor_sprite;
extern s32 g_world_formation_current_menu;
extern s32 g_world_formation_display_mode;
extern world_menu_point_t g_world_formation_cursor_position;
extern u8 g_world_formation_generic_human_graphic_index_table[];
extern world_formation_graphic_entry_t g_world_formation_graphic_entries[];
extern world_menu_input_state_t g_world_formation_menu_input_state;

/* First-word alias retained for the target's separate absolute store. */
extern s32 g_world_formation_menu_new_buttons;
extern void* g_world_formation_ordering_table;
extern u32 g_world_formation_saved_system_flags;

/* Majority spelling: five readers sign-extend as u8. Two files
 * (world_formation_init_selection_cursor, world_formation_step_grid_transition)
 * need s8 to match and keep their own local extern. */
extern u8 g_world_formation_scroll_enabled;
extern u8 g_world_formation_scroll_enabled_latch;
extern u8 g_world_formation_selected_auto_battle_setting;
extern s16 g_world_formation_selected_unit_index;
extern s16 g_world_formation_selected_unit_index_latch;
extern struct world_formation_unit* g_world_formation_temp_unit;
extern u8 g_world_formation_unit_banner_enabled;
extern u8 g_world_formation_unit_browse_enabled;

/* Provisional: month lengths indexed by 1-based month (0x8018deac; follows
 * the zodiac date limits at 0x8018de84). Read by world_unit_update_monster_breeding. */
extern const u8 g_world_month_lengths[WORLD_MONTH_LENGTH_TABLE_COUNT];
extern const world_zodiac_date_limit_t g_world_zodiac_date_limits[WORLD_ZODIAC_DATE_LIMIT_COUNT];
extern s32 g_world_formation_screen_active;
extern s16 g_world_formation_open_request_state;
extern world_formation_texture_page_location_t g_world_formation_view_texture_page;
extern u32 g_world_formation_menu_entry_mask;
extern s32 g_world_formation_name_string_revision;
extern world_formation_graphic_rect_t g_world_formation_view_graphic_rects[];
extern world_formation_fixed_scale_t g_world_formation_view_graphic_offsets[]; /* per-entry 12-bit fixed offsets */
extern u8 g_world_formation_view_graphic_count;                                /* formation graphic entry count */
extern world_formation_view_t g_world_formation_view;

/* Portrait code per sprite set: low byte is the portrait index. */
extern u16 g_world_formation_sprite_by_portrait[];

/* Remaps portrait indices 0x3f and above. */
extern u8 g_world_monster_formation_sprite_table[];
extern u8 g_world_formation_menu_text_table[];
extern u32 g_world_formation_menu_palettes[];
extern u8 g_world_formation_saved_sprite_color[4];
extern s8 g_world_formation_dimmed_sprite_color[];
extern s8 g_world_formation_normal_sprite_color[];
extern u8 g_world_formation_unit_cycle_mode;
extern u8 g_world_formation_cursor_ot_override;
extern u8 g_world_formation_main_menu_initialized;
extern u8 g_world_formation_saved_banner_enabled;
extern u8 g_world_formation_screen_running;
extern u32 g_world_formation_status_label_image[];
extern RECT g_world_formation_status_label_image_rect;
extern world_menu_icon_thread_param_t g_world_formation_egg_triangle_menu;

/* Chained after the egg triangle menu; world_formation_process_frame only resets it. */
extern world_menu_icon_thread_param_t g_world_formation_egg_submenu;
extern u8 g_world_formation_unit_slide_pending;
extern s8 g_world_formation_scroll_position;
extern s8 g_world_formation_scroll_velocity;
extern s8 g_world_formation_selection_cursor_ready;
extern u8 g_world_formation_triangle_menu_open;
extern u8 g_world_formation_triangle_menu_running;
extern volatile u8 g_world_formation_unit_slide_frame;
extern s8 g_world_formation_banner_slide_step;
extern u8 g_world_formation_stat_display_mode;
extern world_draw_number_command_t g_world_formation_gauge_value_number;
extern world_draw_number_command_t g_world_formation_level_number;
extern world_draw_number_command_t g_world_formation_experience_number;
extern world_draw_number_command_t g_world_formation_brave_number;
extern world_draw_number_command_t g_world_formation_faith_number;
extern world_draw_number_command_t g_world_formation_brave_number_wide;
extern world_draw_number_command_t g_world_formation_faith_number_wide;
extern world_draw_number_command_t g_world_formation_gauge_max_number;
extern world_menu_window_command_t g_world_formation_stat_bar_window_top;
extern world_menu_window_command_t g_world_formation_stat_bar_window_bottom;

/* Three consecutive sprite records at 0x8018c704 / 0x8018c718 / 0x8018c72c. */
extern world_gfx_sprite_desc_t g_world_formation_shadow_egg_sprites[3];
extern u8 g_world_formation_gauge_bar_offsets[4];
extern u8 g_world_formation_hp_gauge_colors[12];
extern u8 g_world_formation_mp_gauge_colors[12];
extern u8 g_world_formation_ct_gauge_colors[12];
extern world_gfx_sprite_desc_t g_world_formation_label_cant_equip;
extern world_gfx_sprite_desc_t g_world_formation_label_equipping;
extern world_gfx_sprite_desc_t g_world_formation_label_leaving;
extern world_gfx_sprite_desc_t g_world_formation_unit_dot_sprite;
extern world_gfx_sprite_desc_t g_world_formation_label_missing;
extern world_order_menu_entry_t g_world_formation_panel_windows[];
extern battle_menu_sprite_cell_t g_world_formation_label_tiles[];
extern battle_menu_sprite_cell_t g_world_formation_label_tiles_bottom[];
extern s8 g_world_formation_dot_glow_timer;
extern s8 g_world_formation_dot_glow_direction;
extern u8 g_world_formation_cursor_trail_brightness[8];
extern u8 g_world_formation_grid_transition_frame;
extern u8 g_world_formation_grid_return_frame;
extern u8 g_world_formation_grid_return_done;
extern u8 g_world_formation_portrait_slide_frame;
extern s8 g_world_formation_away_unit_color[];
extern world_menu_icon_thread_param_t g_world_formation_triangle_menu;
extern u8 g_world_formation_dismiss_prompt[];
extern s16 g_world_formation_dismiss_prompt_text_id;
extern s16 g_world_formation_dismiss_prompt_kind;
extern s16 g_world_formation_dismiss_anim_timer;
extern s8 g_world_formation_sort_field_indices[];
extern world_menu_icon_thread_param_t g_world_formation_dismiss_final_confirm_menu;
extern u8 g_world_formation_dismiss_message_wait;
extern u8 g_world_formation_thread_status_snapshot[16];
extern u8 g_world_formation_view_quads[2][0x4b0];
extern world_menu_point_t g_world_formation_cursor_target;
extern world_cursor_trail_point_t g_world_formation_cursor_trail[8];
extern u8 g_world_formation_dismiss_step;
extern u8* g_world_formation_dismiss_menu_data;
extern s16 g_world_formation_record_order[];
extern struct world_formation_unit g_world_formation_unit_records[];
extern u8 g_world_formation_menu_text_buffer[];

/* Signed count of accepted records; pointer entry 20 is a separate extra
 * record at 0x801c9d58, installed by 0x80121fac–0x80121fb8. */
extern s16 g_world_formation_record_count; /* 0x801c8634 */

/* Menu callers cache the builder's return here; rebuilding the record list
 * does not itself refresh this byte. */
extern u8 g_world_formation_unit_count;
extern world_formation_unit_t* g_world_formation_unit_pointers[];
void world_formation_run_screen(s32 tutorial, s32 unused);
s32 world_formation_can_scroll_slots_back(void);
void world_formation_draw_unit_slots(world_menu_point_t* positions, s32 shade);

void world_formation_draw_graphics_below_sprite(
    s16 unit_index, s32 mode, s32 x, s32 y, u8* color, s32 (*get_value)(s16), s32 enabled, s32 priority);

s32 world_formation_run_equip_item_menu(void);
s32 world_formation_run_item_submenu_step(void);
s32 world_formation_run_set_ability_menu(void);
void world_formation_build_job_wheel_availability(void);
void world_formation_build_job_wheel_job_name(void);
s16 world_formation_build_record_list(s32 item_id, world_formation_unit_t** list, s32 mode);

void world_formation_build_unit_graphic_entry(
    s16 formation_index, world_formation_graphic_entry_t* destination, s32 unused);

void world_formation_build_unit_record(
    struct battle_stats* unit, struct world_formation_unit* record, struct party_data* party);

void world_formation_build_view_primitives(world_formation_view_t* view, POLY_FT4* poly);
s32 world_formation_can_dismiss_unit(s16 formation_idx);
s32 world_formation_can_equip_item_in_slot(s16 unit_index, s16 slot, s32 item_id);
s32 world_formation_can_scroll_slots(s32 scroll_position);
s32 world_formation_change_unit_job(s16 unit_id, s16 job_id, s32 update_equipment);
void world_formation_clear_stat_preview(world_item_stat_summary_t* summary, world_item_stat_detail_t* detail);
void world_formation_copy_unit_to_temp(s32 index, s32 unused_slot);
s32 world_formation_cursor_distance_falloff(s32 x, s32 y, s32 base, s32 floor);
void world_formation_cycle_selected_unit(s32 pad_buttons);
void world_formation_dismiss_unit(s16 formation_idx);
void world_formation_draw_background_tiles(s32 y);
void world_formation_draw_cursor_trail(s16* cursor);
void world_formation_draw_job_wheel_sprites(s32 angle, s32 radius_x, s32 radius_y);
void world_formation_draw_sprite_with_shadow(world_gfx_sprite_desc_t* pos, s32 rgb, s32 ot_index);

void world_formation_draw_unit_portrait_frame(
    world_gfx_sprite_desc_t* pos, s32 palette, s32 rgb, s32 hatch_countdown, s32 ot_index);

void world_formation_draw_unit_sprite(s16 unit_index, world_menu_point_t* pos, s8* rgb, s32 ot_index);
s32 world_formation_equip_item_to_unit_slot(s16 unit, s16 slot, s32 item_id);
s32 world_formation_find_index_by_roster_slot(s32 roster_slot);
s32 world_formation_generate_unit(s32 unit_type);

void world_formation_get_generic_human_graphic_entry(
    s32 job_id, s16 is_female, world_formation_graphic_entry_t* destination);

s32 world_formation_get_job_wheel_cursor_job_level(void);
s16 world_formation_get_unit_brave(s32 unit_id);
s16 world_formation_get_unit_experience(s32 unit_id);
s16 world_formation_get_unit_faith(s32 unit_id);
s32 world_formation_get_unit_egg_hatch_total(s32 unit_id);
s32 world_formation_get_unit_egg_hatch_progress(s32 unit_id);
s16 world_formation_get_unit_level(s32 unit_id);
void world_formation_init_cursor_and_scroll_state(s32 formation_index);
void world_formation_init_menu_display(void);
void world_formation_init_menu_state(void);
void world_formation_init_selection_cursor(s16 formation_index);
s32 world_formation_is_job_wheel_selection_available(void);
void world_formation_item_menu(void);
void world_formation_layout_job_wheel_portrait(void);
s32 world_formation_lock_equipment_and_abilities(s32 unit_id);
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
void world_formation_save_records_to_party_data(void);
void world_formation_set_display_mode(s32 mode);
void world_formation_set_menu_entry_mask(u32 mask);
void world_formation_set_ordering_table_pointer(void* ordering_table);
s32 world_formation_set_unit_ability_slot(s16 unit_id, s16 slot, s16 ability, s32 update_equipment);
void world_formation_set_unit_name(s32 formation_index, const u8* name);
void world_formation_stage_selected_unit(void);
void world_formation_start_job_wheel_close(void);
s32 world_formation_step_grid_return_transition(s32 mode);
u8 world_formation_step_grid_transition(s32 mode);
void world_formation_step_job_wheel_close_animation(void);
void world_formation_step_job_wheel_open_animation(void);
void world_formation_step_scroll_velocity(s8* velocity, s8* position);
u8 world_formation_step_unit_portrait_slide(void);
void world_formation_step_unit_selection_slide(void);
void world_formation_stop_menu_threads(void);
void world_formation_update_job_wheel_state(void);
void world_formation_update_open_request(void);
void world_formation_update_unit_selection(void);
void world_formation_view_zoom_thread(void);
void world_formation_update_tutorial_categories_request(void);
void world_formation_update_debug_variable_list_request(void);
void world_formation_update_fixed_number_panel_request(void);
void world_formation_update_world_map_root_request(void);
void world_formation_sort_roster(s32 index);
s32 world_get_formation_unit_stat_by_mode(s32 unit_index, s32 fallback);

/* unit */
/* Provisional: status-billboard record filled by world_menu_copy_unit_data_to_status_billboard
 * (0x800e7c40), the WORLD twin of debugchr_panel_copy_unit_data_to_billboard. */
typedef struct world_unit_status_billboard {
    s16 level;      /* 0x00 */
    s16 team_kind;  /* 0x02; 0 ally, 1 enemy, 2 neutral, 3 auto-battle */
    s16 list_index; /* 0x04 */
    s16 unit_count; /* 0x06 */
    s16 experience; /* 0x08 */
    s16 battle_id;  /* 0x0a */
    u16 hp;         /* 0x0c */
    s16 hp_delta;   /* 0x0e */
    u16 max_hp;     /* 0x10 */
    u16 mp;         /* 0x12 */
    s16 mp_delta;   /* 0x14 */
    u16 max_mp;     /* 0x16 */
    s16 ct;         /* 0x18 */
    s16 _unused_1a; /* 0x1a */
    s16 max_ct;     /* 0x1c; always 100 */
} world_unit_status_billboard_t;

/* Provisional: one 4-byte breeding entry per monster job from 0x5e
 * (0x8018debc): the hatch-day range and the three offspring jobs by rarity
 * class (world_unit_update_monster_breeding). */
typedef struct world_unit_monster_breed_entry {
    u8 hatch_range; /* 0x00: rand() % hatch_range + 1 (at least 2) egg days */
    u8 offspring_job_by_rarity[WORLD_MONSTER_BREED_RARITY_COUNT]; /* 0x01; common through rare */
} world_unit_monster_breed_entry_t;
typedef char world_unit_monster_breed_entry_size_must_be_4[(sizeof(world_unit_monster_breed_entry_t) == 4) ? 1 : -1];

/* The unit status panel's two draw offsets. */
typedef struct world_unit_status_offsets {
    battle_menu_status_panel_draw_offset_t a; /* 0x00 */
    battle_menu_status_panel_draw_offset_t b; /* 0x10 */
} world_unit_status_offsets_t;

/* Provisional: one gauge of the status record. */
typedef struct world_unit_status_gauge {
    s16 value;  /* 0x00 */
    s16 value2; /* 0x02 */
    s16 max;    /* 0x04 */
} world_unit_status_gauge_t;

/* Provisional status record passed to the unit status panel thread body and
 * world_menu_display_hovered_unit_stats; g_world_unit_selected_status_billboard
 * and g_world_unit_comparison_status_billboard are two of them. */
typedef struct world_unit_status_record {
    u8 _unused_00[2];
    s16 status_icon; /* 0x02: 0-3, selects g_world_unit_status_icon_image_params */
    s16 layout;      /* 0x04 */
    u8 _unused_06[4];
    s16 battle_id;                       /* 0x0a */
    world_unit_status_gauge_t gauges[3]; /* 0x0c */
    u8 _unused_1e[4];
} world_unit_status_record_t;
typedef char world_unit_status_record_size_must_be_0x22[(sizeof(world_unit_status_record_t) == 0x22) ? 1 : -1];

/* Unpadded unsigned 16-bit x/y/z triple (6 bytes). */
typedef struct vec3u16 {
    u16 x;
    u16 y;
    u16 z;
} vec3u16_t;
typedef char assert_vec3u16_size[sizeof(vec3u16_t) == 6 ? 1 : -1];

extern world_item_stat_summary_t g_world_selected_unit_stat_summary;
extern world_unit_status_record_t g_world_unit_selected_status_billboard;
extern world_unit_status_record_t g_world_unit_comparison_status_billboard;
extern world_status_frame_t g_world_stat_preview_panel_frames[2];
extern world_unit_editor_panel_data_t g_world_unit_editor_panel_data;

/* The comparison unit's flag and label_text_ids[2], copied by
 * world_gfx_init_screen_setup_2 from its editor load into the second 0x40-byte
 * editor record (0x8013a3c4; this view starts at its +0x0e). */
extern world_unit_editor_panel_data_t g_world_unit_comparison_editor_panel_data;
extern u32 g_world_companion_overlay_state;
extern struct world_item_stat_detail g_world_selected_unit_stat_detail;
extern world_unit_animation_state_t g_world_unit_animation_states[];
extern s16 g_world_unit_comparison_battle_id;
extern u16 g_world_unit_graphics_load_pending;
extern const world_unit_monster_breed_entry_t g_world_unit_monster_breed_table[WORLD_MONSTER_BREED_ENTRY_COUNT];
extern unit_status_staging_t* g_world_unit_status_staging_data;
extern s16 g_world_unit_view_battle_id;
extern u8 g_world_active_unit_data[];

/* Unit editor state; same layout as the EVENT debug character editor's
 * (debugchr_editor_load_selected_unit). */
extern s16 g_world_unit_editor_unit_type;                /* unit type (job id) */
extern s16 g_world_unit_editor_brave;                    /* brave */
extern s16 g_world_unit_editor_faith;                    /* faith */
extern s16 g_world_unit_editor_zodiac;                   /* zodiac */
extern s16 g_world_unit_editor_fields[];                 /* editor unit fields */
extern u8 g_world_unit_editor_primary_skillset_name[];   /* primary skillset name text window */
extern u8 g_world_unit_editor_secondary_skillset_name[]; /* secondary skillset name text window */
extern u8 g_world_unit_editor_unit_name[];               /* name text window */
extern u8 g_world_unit_editor_job_name[];                /* job name text window */
extern u16 g_world_unit_view_started_battle_id;
extern u16 g_world_unit_view_started_comparison_id;
extern s32 g_world_unit_view_mode;
extern s32 g_world_unit_join_screen_active;
extern u16 g_world_unit_status_bounce_offsets[];
extern u16 g_world_unit_status_bounce_step;
extern u16 g_world_unit_status_quad_clut_pairs[][2];
extern u16 g_world_unit_status_sprite_clut_pairs[][2];
extern world_gfx_image_load_parameters_t g_world_unit_status_icon_image_params[];
extern CVECTOR g_world_unit_status_gauge_colors[];
extern world_gfx_image_load_parameters_t* g_world_unit_status_quad_image_params_by_mode[];
extern world_gfx_image_load_parameters_t* g_world_unit_status_sprite_image_params_by_mode[];
extern u8 g_world_unit_status_first_page_by_mode[];
extern RECT g_world_unit_summary_panel_rect;
extern world_gfx_image_load_parameters_t g_world_unit_summary_sprite_image_params[];
extern RECT g_world_unit_editor_panel_rect;  /* panel box */
extern RECT g_world_unit_editor_cursor_rect; /* cursor source */
extern RECT g_world_unit_editor_text_rect_a; /* first text image */
extern RECT g_world_unit_editor_text_rect_b; /* second text image */
extern world_gfx_image_load_parameters_t g_world_unit_editor_sprite_image_params_a[19];
extern world_gfx_image_load_parameters_t g_world_unit_editor_sprite_image_params_b[19];
extern world_gfx_image_load_parameters_t g_world_unit_editor_row_image_params_a[4];
extern world_gfx_image_load_parameters_t g_world_unit_editor_row_image_params_b[4];
extern world_gfx_image_load_parameters_t g_world_unit_editor_item_icon_image_params[5];
extern u8 g_world_unit_editor_line_colors[8 * WORLD_UNIT_EDITOR_LINE_SIZE];
extern u16 g_world_unit_editor_sprite_clut_pairs[19][2];
extern world_unit_view_thread_set_t g_world_unit_view_thread_sets[];
extern u8 g_world_unit_view_panel_fade_modes[];
extern s16 g_world_unit_face_slot_order[];
extern world_unit_status_identity_t g_world_selected_unit_identity;
extern CVECTOR g_world_unit_panel_bar_colors[];
extern world_menu_number_entry_t g_world_selected_unit_number_entries[];
extern world_gfx_image_load_parameters_t g_world_unit_summary_label_sprite_params[];
extern world_gfx_image_load_parameters_t g_world_unit_summary_value_sprite_params[];
extern RECT g_world_unit_status_panel_origin;
extern world_gfx_image_load_parameters_t g_world_unit_status_sprite_params[];
extern u8 g_world_unit_status_portrait_params[];
extern u16 g_world_unit_summary_value_cluts[];
extern u16 g_world_unit_summary_label_cluts[];
extern u8 g_world_unit_summary_mode_row_params[];
extern RECT g_world_selected_unit_portrait_rect;
extern world_status_thread_t g_world_stat_preview_panel_thread_params;
extern world_menu_text_binding_t g_world_selected_unit_text_binding;
extern u8 g_world_unit_status_banner_active;
extern world_menu_entry_t g_world_unit_status_banner_menu;
extern s8 g_world_unit_status_banner_state; /* banner state: 0 idle, 1 finishing, else countdown */

/* Panel scratch areas inside the stack bytes of g_world_thread_contexts[0]
 * (+0x100, +0x200) and [14] (+0x100, +0x200). native_thread_t leaves those
 * bytes unnamed, so a member spelling would be raw offset arithmetic; they
 * stay separate symbols. The palette areas hold one slot per page. */
extern u8 g_world_unit_summary_digit_images[];
extern world_menu_palette_primitives_t g_world_unit_editor_palette;
extern world_menu_palette_slot_t g_world_unit_summary_palettes[];
extern s32 g_world_companion_overlay_prims;
extern u16 g_world_unit_pending_animations[];
extern u16 g_world_unit_was_moving_latch[];
extern vec3u16_t g_world_misc_unit_screen_coords;
extern world_unit_status_page_t g_world_unit_status_panel_pages[];
extern world_unit_editor_frame_t g_world_unit_editor_frames[2];
extern u8 g_world_selected_unit_summary_packets[];
extern u8 g_world_selected_unit_status_packets[];
extern u8 g_world_selected_unit_stat_text_image[];
extern u8 g_world_selected_unit_name_text_image[];
extern u8 g_world_selected_unit_number_text_image[];
extern u8 g_world_selected_unit_portrait_image[];
extern u8 g_world_stat_preview_text_image[];
void set_unit_cannot_exist(s32 unit_index);
struct battle_stats* find_unit_by_id(s32 unit_id, s32* out_index);
s32 world_get_party_unit_formation_sprite(s32 party_index);
void resolve_unit_status_changes(void);
s32 unit_has_adjacent_monster_skill_support(struct battle_stats* unit);
void world_unit_apply_staged_status(s32 unit_id, s32 misc_id);
void world_unit_apply_zalera_draw_status_flags(s32 battle_unit_index, s32 enabled);
void world_unit_build_best_fit_equipment(s16 unit_index, s16* equipment);
s32 world_unit_can_equip_item_id(s16 unit_index, s32 item_id);
s32 world_unit_check_facing_relative_to_camera(s32 unit_id);
void world_unit_clear_status_staging_data(void);
void world_unit_copy_screen_coordinates(u32 misc_id, VECTOR* destination);
void world_unit_copy_screen_data_by_misc_id(s32 misc_id, s16* out);
struct battle_stats* world_unit_get_battle_stats_for_stored(void);
struct battle_stats* world_unit_get_by_unit_id(s32 unit_id);
s32 world_unit_get_map_coords_from_misc_id(s32 misc_id, s16* out);
s32 world_unit_get_portrait_index(s32 roster_slot);
s32 world_unit_get_zodiac_for_date(s32 month, s32 day);
s32 world_unit_has_any_current_status(s32 unit_id);
void world_unit_increase_blue_team_brave_by_10(void);

void world_unit_inflict_status_by_entd_id(
    s32 entd_unit_id, s32 status_index, s32 is_infliction, s32 less_strict_removal);

s32 world_unit_is_moving(s32 id);
void world_unit_join_silently_without_darkscreen(void);
void world_unit_load_selected_into_editor(void);
void world_unit_project_misc_to_screen(s32 misc_id, s16* screen_coords);
void world_unit_reset_animation_states(void);
void world_unit_set_animation(s32 misc_id, u32 value);
void world_unit_set_facing(s32 misc_id, s32 rotation);
void world_unit_start_view_thread(s32 index, s32 unit_a, s32 unit_b);
s32 world_unit_try_get_misc_data_by_id(u16* unit_id, s32* misc_index);
void world_unit_update_staged_status_data(u16 unit_id, u16 restore);
void world_unit_view_supervisor_thread(void);
s32 world_update_unit_status_and_staged_status_data(s32);
s32 world_get_misc_id(s32 unit_id);
s32 world_unit_get_battle_index_by_entd_id(s32 entd_unit_id);
void world_unit_update_monster_breeding(s32 unused);

/* text */
/* WORLD text files materialize 32 section pointers after their 0x80-byte
 * offset/header area. The same table is saved and restored around overlays. */
enum {
    WORLD_TEXT_SECTION_COUNT = 32,
    WORLD_TEXT_SECTION_TABLE_BYTES = 0x80,
    WORLD_TEXT_FILE_HEADER_BYTES = 0x80,
};

/* Provisional: the text draw cursor at 0x8019a22c, passed to the text
 * renderer so it can advance across a line and wrap at the right margin. */
typedef struct world_text_draw_origin {
    u16 x;            /* 0x00 */
    u16 y;            /* 0x02 */
    u8 _unused_04[4]; /* 0x04 */
    s32 right_limit;  /* 0x08: last usable x, minus an 8 pixel margin */
} world_text_draw_origin_t;

/* Provisional: destination placement for the packed-font glyph decoder at
 * 0x80129e4c.  x is a 4bpp pixel column (bit 0 selects the odd nibble),
 * y a row, and stride the destination row pitch in halfwords. */
typedef struct world_text_glyph_target {
    u16 x;      /* 0x00 */
    s16 y;      /* 0x02 */
    s16 stride; /* 0x04: halfwords per row */
} world_text_glyph_target_t;

/* Provisional: the pending text glyph request at 0x8019a338, consumed by
 * world_text_draw_glyph_with_typewriter_delay. The menu text state and the LoadImage RECT follow it
 * in memory but are addressed as separate globals. */
typedef struct world_text_glyph_request {
    u16 x;              /* 0x00: pen x; the low two bits select the sub-byte phase */
    s16 y;              /* 0x02: pen y */
    u16 glyph;          /* 0x04: 0xfa is the blank that skips the upload */
    u8 _unused_06[6];   /* 0x06 */
    s16 bits_per_pixel; /* 0x0c: 0 blanks the image, 0x10 draws it */
    s16 palette;        /* 0x0e */
} world_text_glyph_request_t;

/* Pending message-box glyph (0x801c3474), written by world_text_message_box_thread
 * and drawn by world_text_draw_glyph_with_typewriter_delay_2. */
typedef struct world_glyph_draw_state {
    u16 pixel_x;      /* 0x00 */
    u16 y;            /* 0x02; the text cursor y */
    u16 code;         /* 0x04 */
    u8 _unused_06[6]; /* 0x06 */
    s16 mode;         /* 0x0c */
    s16 color;        /* 0x0e */
} world_glyph_draw_state_t;

/* Provisional: menu text origin/stride words at 0x80153280 (the WORLD
 * script variable region; world_menu_set_text_origin writes +0x04/+0x06).
 * The text-image builders address these through one base register, so they
 * are modelled as one aggregate; the two parameter words at 0x80153304 and
 * 0x80153308 schedule as plain scalars and stay separate globals. */
/* The world state block at 0x80153280. Its first word is the script-variable
 * pointer, which world_init_scene_bindings points at g_word_flags; code that
 * only needs that word binds the same address as g_world_script_variables. */
typedef struct world_menu_text_state {
    s32* script_variables; /* 0x00; also bound as g_world_script_variables */
    s16 origin_x;          /* 0x04 (0x80153284) */
    s16 origin_y;          /* 0x06 (0x80153286) */
    s32 _unused_08;
    s32 stride; /* 0x0c (0x8015328c): rect width * 4 */
    s32 color;  /* 0x10 (0x80153290): glyph blit fill; 0x44444444 while text colour 4 is drawn
                   (world_menu_draw_text_columns) */
} world_menu_text_state_t;

extern world_glyph_draw_state_t g_world_text_message_box_glyph;
extern s32 g_world_text_item_names;
extern u8 g_world_numeric_display_palette_indices[12];
extern u8 g_world_text_glyph_widths[];
extern s32 g_world_text_line_widths[];
extern s32 g_world_text_overlay_active;
extern u8 g_world_text_player_name_buffer[20];
extern u8* g_world_text_saved_section_pointers[32];
extern s32 g_world_text_section_offsets[32];
extern u8* g_world_text_section_pointer_backup[32];
extern s32 g_world_text_section_pointers_saved;
extern s32 g_world_text_substitution_values[];

/* Text substitutions $7E/$7F (0x801533d0, table index 0x7e from 0x801531d8)
 * as the four halfwords world_text_set_substitution_values_7e_7f stores. */
extern RECT g_world_text_substitution_rect;
extern char g_world_text_wait_for_allocation_message[];
extern char g_world_text_decimal_format[];
extern char g_world_text_wait_for_file_allocation_message[]; /* "WAIT FOR FILEALLOCATION\n" */
extern char g_world_text_reading_character_message[];        /* "reading charactor%d \n" */
extern u8 g_world_text_glyph_bitmaps[];
extern s32 g_world_text_glyph_first_row;
extern s32 g_world_text_glyph_row_limit;
extern s16 g_world_text_speaker_swap_unit_a;
extern s16 g_world_text_speaker_swap_unit_b;
extern s32 g_world_text_glyphs_per_wait;      /* frames a glyph stays up before the typewriter advances */
extern u8 g_world_text_slot_source_columns[]; /* per-slot source column, stride 1 */
extern u8 g_world_text_slot_row_pitches[];    /* per-slot source row pitch in bytes, stride 1 */
extern u32 g_world_text_decimal_divisors[];
extern s32 g_world_text_current_line;
extern RECT g_world_numeric_display_frame_rect;
extern RECT g_world_numeric_display_texture_window;
extern RECT g_world_numeric_display_text_rect_a;
extern RECT g_world_numeric_display_text_rect_b;
extern RECT g_world_numeric_display_text_rect_c;
extern world_gfx_image_load_parameters_t g_world_numeric_display_image_params[18];
extern world_menu_number_entry_t g_world_numeric_display_entries[];
extern u16 g_world_text_message_duration_frames;

/* Same layout as g_world_decimal_glyph_source. */
extern world_glyph_blit_t g_world_text_large_number_glyph_rect;
extern world_glyph_blit_t g_world_text_shadow_bottom_glyph_source[];
extern world_glyph_blit_t g_world_text_shadow_top_glyph_source[];
extern s16 g_world_fixed_number_panel_request_state;
extern s32 g_world_text_message_box_digit_divisors[];
extern s32 g_world_text_message_box_slot_threads[3];
extern s32 g_world_text_message_box_slot_glyph_counters[3];
extern s32 g_world_text_message_box_glyph_delay;
extern u8 g_world_text_message_box_slot_carry_offset[4];
extern u8 g_world_text_message_box_slot_prev_width[4];

/* Same layout as world_text_render_decimal_entry_list's output record. */
extern world_glyph_blit_t g_world_decimal_glyph_source;
extern world_status_thread_t g_world_numeric_editor_thread_params;
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
extern world_menu_palette_slot_t g_world_numeric_display_palettes[];
extern world_text_draw_origin_t g_world_text_draw_origin;
extern world_text_glyph_request_t g_world_text_typewriter_glyph;
extern world_menu_text_state_t g_world_text_typewriter_state; /* origin/stride passed to 0x800ff284 */
extern RECT g_world_text_typewriter_vram_rect;                /* VRAM destination of the finished glyph */
extern u8 g_world_text_typewriter_glyph_images[3][8][0x54];   /* eight glyph images per slot */
extern u8 g_world_text_typewriter_previous_images[3][0x54];   /* previous image per slot */
extern u8 g_world_text_typewriter_column_scratch[14];         /* one-column scratch used by the phase shift */
extern s16 g_world_text_partner_screen_coords[];
extern s16 g_world_text_speaker_screen_coords[3];
extern world_panel_record_t g_world_numeric_display_panel_records[2];
extern s32 g_world_text_substitution_values_backup[32]; /* substitution words saved around the text calls */
extern world_menu_list_page_t g_world_text_list_pages[2];

/* The two glyph blit descriptors, filled by world_gfx_set_blit_source_rect
 * and world_gfx_set_blit_dest_position. Each is 0x10 bytes and the second
 * starts 0x10 past the first. The stride is the surface width in pixels:
 * 0x100 for the glyph sheet, 0x80 for a dead-unit panel's scratch image. */
extern world_glyph_blit_t g_world_text_glyph_source;
extern world_glyph_blit_t g_world_text_glyph_dest;
extern RECT g_world_text_message_box_vram_rect;
extern u8 g_world_text_message_box_glyph_images[3][8][84];
extern u8 g_world_text_message_box_previous_images[3][84];
extern u8 g_world_text_message_box_column_scratch[14];
extern s32 g_world_text_thread_active_text;
extern u8 g_world_text_row_image_buffer[];
extern u8* g_world_text_glyph_bitmap_data;

/* Text section base per text-id section (id >> 11 & 0x1f); several
 * elements also carry their own names below. */
extern u8* g_world_text_section_pointers[WORLD_TEXT_SECTION_COUNT];
extern u8* g_world_text_job_names;
extern u8* g_world_text_roster_unit_names;
extern u8* g_world_text_unit_names;
extern u8* g_world_text_ability_names;
extern void* g_world_text_location_names;
extern u8* g_world_text_skillset_names;
extern void* g_world_text_message_section;
void world_text_message_box_thread(void);
void world_text_blit_font_glyph_to_4bpp(u8* glyph, void* arg1, u16* x, s32 arg3);
s32 world_text_decode_font_glyph_to_4bpp(s32 glyph, u8* image, world_text_glyph_target_t* target, s32 palette);
void world_text_build_display_message(s32 offset, s32 flags, s32* out_width);
u8* world_text_advance_cursor_with_backreferences_2(world_text_backreference_state_t* state, u8* cursor);
void world_text_blit_glyph(void* otag, void* text_id, world_glyph_blit_t* resource, world_glyph_blit_t* pos);
s32 world_text_count_decimal_digits(s32 value);
void world_text_draw_glyph_with_typewriter_delay(s32 pen_x, s32 pen_y, s32 delay);
void world_text_draw_glyph_with_typewriter_delay_2(s32 x, s32 y, s32 wait);
void world_text_draw_shadowed(void* buffer, world_glyph_blit_t* dest);
u8* world_text_find_entry_by_index(u8* p, s16 count, s32 unused);

void world_text_render_decimal_entry_list(
    s32 buffer, world_menu_number_entry_t* entries, world_glyph_blit_t* output, s32 count);

void world_text_render_decimal_value(s32 value, s32 flags, void* pixels, world_glyph_blit_t* position);
void world_text_render_decimal_value_or_dashes(s32 value, s32 flags, void* pixels, world_glyph_blit_t* position);
void world_text_render_id_list_to_image_rows(u8* font, s16* text_ids, RECT* destination, s32 palette);

void world_text_render_ids_into_image(
    u8* image, RECT* rect, s32 unused, s32 max_glyphs, u8* font, s16* ids, s32 count, s16 glyph);

void world_text_render_signed_gauge_entry_list(
    s32 buffer, world_menu_number_entry_t* entries, world_glyph_blit_t* output, s32 count);

s32 world_text_adjust_value_for_event_code(s32 event_code, s32 value);
u8* world_text_advance_cursor_with_backreferences(world_text_backreference_state_t* state, u8* cursor);
u8* world_text_advance_entry_by_char_count(u8* text, s32 count);
void world_text_concatenate_entries(u8* text_table, u8* destination, const s16* text_ids, s32 separate_entries);
u8* world_text_find_entry(s32 combined_text_index);
void world_text_determine_spell_quote(world_unit_command_action_t* action, s32 unit_id, s32 enabled);
void world_text_generate_battle_unit_name_string(void);
void world_text_generate_formation_unit_name_string(void);
const u8* world_text_skip_to_entry(const u8* text, s32 entry_index);
s16 world_text_get_encoded_char_at_position(u8* text, s16 position, s32 mode);
void world_text_init_format_section_pointers(s32* offset_table);
void world_text_init_section_pointers(void);
s32 world_text_is_printing(s32 thread_id);
void world_text_kern_into_command_buffer(u8* text, u16* name, RECT* rect, s32 palette);
void world_text_measure(s16* columns, s16* rows, const u8* text);
s32 world_text_measure_entry_pixel_width(u8* entry);
void world_text_measure_pixels(s16* width, s16* rows, const u8* text);
void world_text_restore_section_pointers(void);
void world_text_restore_sections_and_stop_thread(void);
void world_text_resume_printing(s32 thread_id);
void world_text_run_overlay_thread(void);
void world_text_save_section_pointers(void);
void world_text_set_message_duration_frames(s32 frames);
void world_text_show_message_and_play_sound(s32 message_id, s32 sound_id);
void world_text_start_character_thread_if_idle(s32 thread_id, s32 first, s32 second, s32 third);
void world_text_try_start_overlay_thread(void);
void world_text_character_handling_thread(void);

/* menu */
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

/* Provisional: menu-thread data record reached through
 * g_world_menu_thread_menu_data; the thread entry at 0x28 and the
 * selection at 0x74 are the only known fields. */
typedef struct world_menu_thread_data {
    u8 _unused_00[0x28];
    void (*thread_entry)(void); /* 0x28 */
    u8 _unused_2c[0x74 - 0x2c];
    s16 selection; /* 0x74 */
} world_menu_thread_data_t;

/* Variable-length list-layout command in a WORLD menu script. */
typedef struct world_menu_list_record {
    u8 type;   /* 0x00: 0x10 = list layout, 0x1c = end of stream */
    u8 length; /* 0x01: byte length of this record */
    u8 _unused_02[2];
    u8 x;            /* 0x04 */
    u8 row_height;   /* 0x05 */
    u8 visible_rows; /* 0x06 */
    u8 _unused_07[2];
    u8 width_a; /* 0x09 */
    u8 width_b; /* 0x0a */
} world_menu_list_record_t;

typedef struct world_menu_color_input {
    u8 _unused_00[0x10];
    s32 style; /* 0x10: 1 selects palette bank 1 (battle_menu_status_panel_frame_config_t.style) */
} world_menu_color_input_t;

typedef char world_menu_line_f2_size_must_be_16[(sizeof(LINE_F2) == 16) ? 1 : -1];

/* Provisional record handed to the world text character thread by
 * world_menu_start_description_text_thread; only the description text buffer at +0x38 is known. */
typedef struct world_menu_description_record {
    u8 _unused_00[0x38];
    u8 description_text[1]; /* 0x38: rendered menu description text */
} world_menu_description_record_t;

/* Provisional record whose cursor is stepped with wrap-around by
 * world_menu_step_wrapping_cursor_on_scroll_buttons; only the inclusive upper cursor bound at +0x1e is
 * known. */
typedef struct world_menu_wrapping_cursor_bounds {
    u8 _unused_00[0x1e];
    s16 max_index; /* 0x1e: cursor wraps between 0 and this value */
} world_menu_wrapping_cursor_bounds_t;

/* Provisional record read by world_menu_build_layout_sprites: an x/y origin at +0x08
 * and a layout mode halfword at +0x2c (1 or 2). Other fields are unknown. */
typedef struct world_menu_sprite_layout {
    u8 _unused_00[8];
    u16 x; /* 0x08 */
    u16 y; /* 0x0a */
    u8 _unused_0c[0x2c - 0x0c];
    s16 mode; /* 0x2c */
} world_menu_sprite_layout_t;

/* Provisional record rendered by the WORLD menu-text image builders at
 * 0x800e4b04 / 0x8010e2c8: a text image buffer is allocated (16-colour via
 * 0x800ff690, otherwise 0x800ff9ac), the text at +0x10 is drawn through
 * the main-thread text callback, and the RECT at +0x08 is uploaded. */
typedef struct world_menu_text_image {
    void* buffer; /* 0x00 */
    u16 width;    /* 0x04 */
    u16 height;   /* 0x06 */
    RECT rect;    /* 0x08 */
    void* text;   /* 0x10 */
    u8 _unused_14[2];
    u16 dialog_type;    /* 0x16: window-image mode (DisplayMessage Dialog Type) */
    u16 bits_per_pixel; /* 0x18: 0 skips rendering, 0x10 selects the wide builder */
    u8 _unused_1a[8];
    u16 first_line; /* 0x22: first line world_menu_display_text draws */
    u16 last_line;  /* 0x24: line world_menu_display_text stops at */
    u8 _unused_26[0x30 - 0x26];
    s16 tail_offset; /* 0x30: dialog tail offset from the window centre */
    u8 _unused_32[0x40 - 0x32];
    u16 origin_x; /* 0x40 */
    u16 origin_y; /* 0x42 */
} world_menu_text_image_t;

/* Provisional: first thread parameter of world_menu_input_loop_thread's menu loop
 * (also passed to world_menu_handle_entry_confirm). Only the fields touched by
 * world_menu_cancel_thread_group (0x800ebe7c) are known. */
typedef struct world_menu_cancel_context {
    u8 _unused_00[0x20];
    s16 thread_count; /* 0x20; -1 when there is no thread group */
    u8 _unused_22[0x34 - 0x22];
    s16* result_pointer; /* 0x34; set to -1 on cancel */
} world_menu_cancel_context_t;

/* Partial image view used by 0x801061e0. Upload rectangles determine the
 * second page's size; it begins at byte +0x8000 in both resource layouts. */
typedef struct world_menu_image_pages {
    u32 first_page[8192];
    u32 second_page[1];
} world_menu_image_pages_t;

/* The 0x9800-byte font resource uploads pixels at +0x1000 and palettes at
 * +0x9000/+0x9200. Its leading 0x1000 bytes are not uploaded here. */
typedef struct world_menu_font_image {
    u8 prefix[4096];
    u32 pixels[8192];
    u32 palettes_a[128];
    u32 palettes_b[384];
} world_menu_font_image_t;

typedef char world_menu_font_image_size[(sizeof(world_menu_font_image_t) == 0x9800) ? 1 : -1];

/* Persistent palette source, separate from the temporary font resource.
 * Only the 64-byte color block at +0x1000 is established. */
typedef struct world_menu_palette_source {
    u8 prefix[4096];
    u8 colors[64];
} world_menu_palette_source_t;

/* 17-byte per-unit menu row at 0x8013a750, 21 rows (one per battle unit slot).
 * The BATTLE twin battle_menu_reset_unit_records (0x8013f8b4) proves the
 * 17-byte stride and the entry count for the same table. */
typedef struct world_menu_record {
    u8 bytes[17];
} world_menu_record_t;
typedef char world_menu_record_size_must_be_17[(sizeof(world_menu_record_t) == 17) ? 1 : -1];

/* Selection caches read the low byte of a little-endian halfword. */
typedef union world_menu_halfword_view {
    s16 value;
    u8 bytes[2];
} world_menu_halfword_view_t;

/* Menu allocator slot, 16 at 0x8019b368; buffer is -1 when the slot is free.
 * WORLD twin of the BATTLE menu allocator slot. */
typedef struct world_menu_allocation {
    u8* buffer;
    s32 page_count;
} world_menu_allocation_t;
typedef char world_menu_allocation_size_must_be_8[(sizeof(world_menu_allocation_t) == 8) ? 1 : -1];

/* Per-menu command remap at 0x8013a658 (31 entries; the first 12 are the
 * stored-unit menus): map[0] handles 0xff, map[n + 1] other values. */
typedef struct world_menu_command_map {
    u16 menu_id;
    u16 _padding_02; /* aligns map */
    u8* map;
} world_menu_command_map_t;

/* Value range of a numeric menu entry (world_menu_number_entry_thread and
 * world_menu_labeled_number_entry_thread). */
typedef struct world_menu_number_range {
    s16 min;                 /* 0x00 */
    s16 max;                 /* 0x02 */
    s16 _unknown_04[2];      /* 0x04: -1 from wldcore_window_init_number_panel_render_thread */
    void (*on_change)(void); /* 0x08 */
} world_menu_number_range_t;

/* Shared window record whose signed rectangle the window-frame and tiled
 * rectangle commands use for source 2 (pointer at 0x801cd720). */
typedef struct world_menu_window_rect_source {
    u8 _unused_00[6];
    RECT rect; /* 0x06 */
} world_menu_window_rect_source_t;

/* The menu thread's first parameter as the dead-unit panels read it. Only the
 * leading window rectangle is used and the record is certainly longer, so no
 * size is asserted. The fields are unsigned: every read in the target is an
 * lhu, and making them signed turns each one into an lh. */
typedef struct world_menu_window {
    u16 x;
    u16 y;
    u16 w;
    u16 h;
} world_menu_window_t;

/* The primitive block at 0x801cc074: the dead-unit and window-frame panels
 * lay it out as two panel frames, world_menu_build_sprite_page as two sprite
 * pages. */
typedef union world_menu_panel_buffer {
    world_panel_frame_t panel_frames[2];
    menu_sprite_page_t sprite_pages[2];
} world_menu_panel_buffer_t;

/* Provisional: the AT-list work area world_menu_build_ability_preview_at_list
 * fills, reached through g_event_overlay_load_address. The five parallel
 * halfword columns become the layout's text_ids/text_colors tables, `text` is
 * published through g_world_text_section_pointers[31], and `request` is the block handed to the
 * main-stack preview call, whose descriptor array follows it. */
typedef struct world_menu_at_list_work {
    s16 primary_values[40];                /* 0x000: turn-value column */
    s16 secondary_values[40];              /* 0x050: unit-name column */
    s16 entry_numbers[40];                 /* 0x0a0: ordinal column */
    s16 primary_colors[40];                /* 0x0f0 */
    s16 secondary_colors[40];              /* 0x140 */
    u8 text[0x4f0];                        /* 0x190: packed 0xfe-terminated names */
    s32 request;                           /* 0x680: preview request header */
    world_at_descriptor_t descriptors[40]; /* 0x684 */
} world_menu_at_list_work_t;
typedef char world_menu_at_list_work_descriptors_offset_must_be_0x684
    [((unsigned long)&((world_menu_at_list_work_t*)0)->descriptors == 0x684) ? 1 : -1];

/* Result slots the shop menus share. */
enum {
    WORLD_MENU_SELECTION_SHOP_QUANTITY = 3, /* chosen purchase/sale quantity */
    WORLD_MENU_SELECTION_PROMPT_RESULT = 4, /* yes/no prompt result (0 confirms) */
};

/* Provisional: 12-byte textured-sprite source record filled by
 * world_item_build_icon_source and world_item_build_type_icon_source and
 * consumed by the menu icon strip at 0x801c8464. */
typedef struct world_item_icon_source {
    s16 u;     /* 0x00 */
    s16 v;     /* 0x02 */
    s16 w;     /* 0x04 */
    s16 h;     /* 0x06 */
    u16 clut;  /* 0x08 */
    u16 tpage; /* 0x0a */
} world_item_icon_source_t;
typedef char world_item_icon_source_size_must_be_12[sizeof(world_item_icon_source_t) == 12 ? 1 : -1];

extern u8 g_menu_controller_input_0[];
extern u8 g_menu_controller_input_1[];
extern u8 g_world_menu_buffer_allocation_map[0x70];
extern u16 g_world_menu_clut;
extern u16 g_world_menu_clut_back;
extern u16 g_world_menu_clut_back_alt;
extern u16 g_world_menu_clut_front;
extern u16 g_world_menu_clut_front_alt;
extern s32 g_world_menu_color_blue;
extern s32 g_world_menu_color_green;
extern s32 g_world_menu_color_red;
extern u32 g_world_menu_current_button_input;
extern s32 g_world_menu_current_id;
extern s16 g_world_menu_cursor_position;
extern s16 g_world_menu_cursor_positions[16];
extern s32 g_world_menu_description_text_id;
extern s16 g_world_menu_draw_priority;
extern s32 g_world_menu_event_state_flag;
extern u16 g_world_menu_icon_tpage;
extern u16 g_world_menu_input_disabled;
extern u32 g_world_menu_input_repeat_counter_0;
extern u32 g_world_menu_input_repeat_counter_1;
extern u32 g_world_menu_input_repeat_counter_2;
extern s16 g_world_menu_learn_unlocked_jobs[];
extern s32 g_world_menu_main_scroll_direction;
extern s32 g_world_menu_main_scroll_threshold;
extern s16 g_world_menu_option_count;
extern s16 g_world_menu_order_unit_cursor;
extern s16 g_world_menu_order_unit_entries[];
extern s8 g_world_menu_order_unit_open;
extern s32 g_world_menu_packet_buffer_index;
extern u16 g_world_menu_palette_clut;
extern u8 g_world_menu_palette_colors[];
extern s32 g_world_menu_panel_fade_intensity;
extern s32 g_world_menu_panel_fade_mode;
extern u32 g_world_menu_previous_button_input;
extern u8 g_world_menu_prompt_active;
extern const u8 g_world_menu_record_template[8];
extern u8 g_world_menu_restore_pending;
extern u16 g_world_menu_row_height;
extern s16 g_world_menu_screen_open_request;
extern u8* g_world_menu_scripts[];
extern s16 g_world_menu_scroll_offset;
extern s16 g_world_menu_scroll_pixel_offset;
extern s32 g_world_menu_scroll_row_offset;
extern u8 g_world_menu_sound_effect_id;
extern s32 g_world_menu_sound_muted;
extern CVECTOR g_world_menu_sprite_color;
extern world_menu_entry_t g_world_menu_system_entries[];
extern s8 g_world_menu_temporary_draw_area_active;
extern s32 g_world_menu_text_color;
extern s32 g_world_menu_text_file_buffer[];
extern s32 g_world_menu_text_param_22;                  /* 0x80153304: record +0x22 */
extern s32 g_world_menu_text_param_24;                  /* 0x80153308: record +0x24 */
extern world_menu_text_state_t g_world_menu_text_state; /* 0x80153280 */
extern u16 g_world_menu_texture_page;
extern s32 g_world_menu_thread_result;
extern s32 g_world_menu_thread_running_status;
extern u16 g_world_menu_transition_active;
extern u16 g_world_menu_transition_timeout;
extern world_menu_record_t g_world_menu_unit_selection_rows[];
extern s32 g_world_menu_use_alternate_main_palette;
extern u8 g_world_menu_use_alternate_palette;
extern s16 g_world_menu_use_scroll_position;
extern menu_window_buffer_t* g_world_menu_window_buffer_pointers[6];
extern s8 g_world_menu_window_scale_step;
extern u16 g_world_menu_ability_display_flags[20];
extern s32 g_world_menu_resolved_skillset;
extern s32 g_world_menu_resolved_ability;
extern s32 g_world_menu_resolved_command;
extern s32 g_world_menu_last_dispatched_command;
extern s32 g_world_menu_last_dispatched_option;
extern u8 g_world_menu_restore_depth;
extern u8 g_world_menu_restore_unit_id;
extern world_menu_command_map_t g_world_menu_command_maps[31];
extern world_menu_palette_source_t g_world_menu_glyph_image[];
extern u8 g_world_menu_default_palette_colors[];
extern RECT g_world_menu_portrait_pixel_rect;
extern RECT g_world_menu_portrait_palette_rect;
extern s32 g_world_menu_scrolling_list_depth;
extern s32 g_world_menu_hide_numeric_values;
extern s16 g_world_menu_overlay_state;
extern u16 g_world_menu_transition_finished;
extern u16 g_world_menu_input_block_frames;
extern s32 g_world_menu_slot_owner_thread_ids[];
extern s32 g_world_menu_slot_states[];

/* Twelve crop percentages, indexed by step. */
extern u16 g_world_menu_cursor_zoom_percentages[];
extern world_menu_scroll_text_layout_t g_world_menu_status_list_layout;
extern world_menu_scroll_text_layout_t g_world_menu_at_list_layout;
extern s16 g_world_menu_auto_battle_row_actions[8];
extern s16 g_world_menu_action_slot_row_actions;
extern s16 g_world_menu_skillset_row_actions[]; /* per-row message id */
extern s16 g_world_menu_wait_row_actions[4];
extern u16 g_world_menu_status_list_row_actions[8];
extern u16 g_world_menu_system_function_row_actions[];
extern s16 g_world_menu_confirm_stored_unit_row_actions;
extern s16 g_world_menu_result; /* menu result: -4 pending, -1 cancelled */
extern s16 g_world_menu_set_auto_battle_row_actions;
extern RECT g_world_menu_numeric_display_texture_window; /* texture window for the menu page */

extern s8
    g_world_menu_numeric_display_frame_offsets[]; /* twelve world_menu_line_layout_t records, walked by byte offset */

extern u16* g_world_menu_window_tween_easing_tables[];
extern u16 g_world_menu_hovered_stats_frame_count;
extern world_menu_quad_piece_t g_world_menu_quad_pieces[]; /* quad piece table */
extern u8 g_world_menu_quad_group_starts[];                /* first piece index per group; [n + 1] ends group n */
extern world_menu_quad_colors_t g_world_menu_quad_colors;  /* vertex colours */
extern world_menu_quad_origin_t g_world_menu_quad_origin;  /* screen origin */
extern RECT g_world_menu_sprite_page_texture_windows[5];
extern world_gfx_image_load_parameters_t g_world_menu_sprite_page_image_params[30];
extern RECT g_world_menu_hovered_unit_stats_display;
extern RECT g_world_menu_attack_caster_stats_display;
extern RECT g_world_menu_attack_target_stats_display;
extern RECT g_world_menu_right_unit_stats_display;
extern world_menu_number_entry_t g_world_menu_hovered_unit_stats_setup[];
extern world_menu_number_entry_t g_world_menu_right_unit_stats_setup[];
extern world_menu_number_entry_t g_world_menu_attack_caster_stats_setup[];
extern world_menu_number_entry_t g_world_menu_attack_target_stats_setup[];

/* Four image-load parameter blocks per icon kind, for kinds 5..15. */
extern world_gfx_image_load_parameters_t g_world_menu_icon_strip_image_params[];

/* One image-load parameter block per single-icon kind 0..4; index 1 doubles as
 * the arrow placement used by kind 0x10. */
extern world_gfx_image_load_parameters_t g_world_menu_single_icon_image_params[];
extern s32 g_world_menu_building_thread_started;
extern u8 g_world_menu_cleared_entry_indices[11];
extern s16 g_world_menu_row_type_confirm_actions[][2]; /* per row type: [1] == 1 lists by id */
extern u8 g_world_menu_action_slot_default_messages[];
extern RECT g_world_menu_number_glyph_rect; /* glyph source rectangle in the number/sign font */

/* Tail glyph source and destination records for world_menu_build_window_image. */
extern world_glyph_blit_t g_world_menu_window_sprite_source;
extern world_glyph_blit_t g_world_menu_window_sprite_position;
extern RECT g_world_menu_panel_texture_windows_b[7];
extern RECT g_world_menu_panel_texture_windows[7];
extern u8 g_world_menu_at_list_entry_table[];
extern world_menu_entry_t g_world_menu_at_list_state;
extern u8* g_world_menu_at_list_entry_table_pointer;
extern u16 g_world_menu_transition_duration;
extern s16 g_world_menu_at_list_menu_type;
extern s16 g_world_menu_open_entry_index;
extern s32 g_world_menu_thread_group_parameter;
extern s32 g_world_menu_location_id;
extern s32 g_world_menu_location_window_x;
extern s32 g_world_menu_location_window_y;
extern u32 g_world_menu_default_entry_0_text_values[2]; /* text index + 0xa000, value */
extern s32 g_world_menu_default_entry_1_text_values[2]; /* value, 0 */
extern s32 g_world_menu_location_text_id;
extern u16 g_world_menu_location_list_text_ids[];
extern s16 g_world_menu_location_list_row_colors[];
extern world_menu_scroll_text_layout_t g_world_menu_location_list_layout;
extern u8 g_world_menu_submenu_page_counts[];
extern u8* g_world_menu_submenu_page_text_ids[];
extern world_menu_scroll_text_layout_t g_world_menu_submenu_page_layout;
extern s32 g_world_menu_number_entry_value;
extern world_menu_entry_t g_world_menu_default_entries[];
extern RECT g_world_menu_frame_palette_rect;
extern RECT g_world_menu_frame_palette_tail_rect;
extern RECT g_world_menu_frame_image_rect;
extern RECT g_world_menu_item_image_rect;
extern RECT g_world_menu_common_clut_rect;
extern RECT g_world_menu_item_palette_rect;
extern RECT g_world_menu_unit_image_rect_a;
extern RECT g_world_menu_unit_image_rect_b;
extern u32 g_world_menu_common_clut_data[];
extern const u16 g_world_menu_zoom_percentages[12]; /* twelve crop percentages, indexed by step */
extern s16* g_world_menu_entry_text_id_tables[];    /* halfword tables selected by entry select_text_table */
extern s32 g_world_menu_number_entry_digit_steps[];
extern s32 g_world_menu_indicator_packet_index[2];
extern s32 g_world_menu_indicator_brightness[2];
extern s32 g_world_menu_indicator_fade_work[2];
extern s32 g_world_menu_indicator_state[2];

/* Per-piece 4.12 fixed-point x/y scale; a negative factor flips that axis. */
extern point16_t g_world_menu_button_piece_scales[];
extern u8 g_world_menu_button_piece_starts[];
extern s8 g_world_menu_button_press_timers[];
extern u16 g_world_menu_window_tpage;
extern u16 g_world_menu_palette_clut_normal;
extern u16 g_world_menu_palette_clut_alt;
extern u16 g_world_menu_alternate_number_clut_alt;
extern u16 g_world_menu_alternate_number_clut_normal;
extern u16 g_world_menu_disabled_number_clut_normal;
extern u16 g_world_menu_disabled_number_clut_alt;
extern u16 g_world_menu_button_clut;
extern u16 g_world_menu_hp_gauge_clut;
extern u16 g_world_menu_mp_gauge_clut;
extern u16 g_world_menu_ct_gauge_clut;
extern u16 g_world_menu_background_clut;
extern u16 g_world_menu_window_clut_alt;
extern u16 g_world_menu_window_clut_normal;
extern u8* (*g_world_menu_script_handlers[])(u8* script);
extern u8 g_world_menu_script_sprite_command[];
extern world_menu_window_command_t g_world_menu_window_interior_command;
extern s16 g_world_menu_window_scale_table[];
extern world_gfx_sprite_desc_t g_world_menu_number_blank_sprite;
extern u8 g_world_menu_cursor_bob_thresholds[];
extern u8 g_world_menu_cursor_bob_active_thresholds[];
extern s16 g_world_menu_list_row_text_ids[];
extern u16 g_world_menu_list_cursor_anim;
extern u16 g_world_menu_scroll_hold_counter;
extern s16 g_world_menu_text_redraw_request;
extern s16 g_world_menu_pending_selection[4];
extern u16 g_world_grid_menu_cursor_anim; /* grid cursor animation state */
extern u8 g_world_grid_menu_initialized;  /* grid menu initialised */
extern s32 g_world_menu_new_button_input;
extern s16 g_world_menu_status_left_text_ids[20];
extern s16 g_world_menu_status_right_text_ids[20];
extern struct battle_ai_command_action g_world_menu_preview_action;
extern s32 g_world_menu_monster_skillset_flag;
extern u16 g_world_menu_anything_ability_id;
extern s16 g_world_menu_selected_option;
extern s16 g_world_menu_action_slot_selected_option;
extern world_menu_allocation_t g_world_menu_buffer_allocations[16];
extern u8 g_world_menu_buffer_arena[0x70][0x100];
extern s32 g_world_menu_preview_target_action;
extern world_fade_tile_frame_t g_world_menu_panel_fade_frames[];
extern s32 g_world_active_menu; /* active menu */
extern u8 g_world_menu_text_page_image[];
extern u8 g_world_menu_window_frame_image[];
extern s16 g_world_menu_at_list_primary_values[];
extern s16 g_world_menu_at_list_secondary_values[];
extern s16 g_world_menu_at_list_indices[];
extern s16 g_world_menu_at_list_flags[256];
extern u8 g_world_menu_at_list_text[];
extern s16 g_world_menu_submenu_page_row_text_ids[];
extern u32* g_world_menu_list_controller_input;
extern world_item_icon_source_t g_world_menu_entry_item_icon_source;
extern u16 g_world_menu_scroll_repeat_timer;
extern s32 g_world_menu_display_script_input;
extern s16 g_world_menu_list_visible_rows;
extern s16 g_world_menu_list_row_record_count;
extern s16 g_world_menu_semi_trans;
extern s16 g_world_menu_window_right_x;
extern u8 g_world_menu_list_layout_pending;
extern const s16* g_world_menu_list_entry_ids;
extern u32 g_world_menu_list_text_table;
extern u16 g_world_menu_list_image_width;
extern u8 g_world_menu_list_glyph_width;
extern u8 g_world_menu_list_text_columns;
extern u8 g_world_menu_list_scroll_direction;
extern RECT g_world_menu_window_rect;
extern u8 g_world_grid_menu_selected_cell; /* selected cell */
extern world_menu_panel_buffer_t g_world_menu_panel_buffer;
extern u16 g_world_menu_alternate_number_clut;
extern s16 g_world_menu_entry_ids[];
extern s8 g_world_menu_music_slot; /* formation music slot */
extern u16 g_world_menu_window_clut;
extern u16 g_world_menu_active_clut_front;
extern u16 g_world_menu_active_clut_back;
extern world_menu_window_rect_source_t* g_world_menu_window_rect_source;

/* Result slot per menu (-1 while none); world_menu_reset_selection_results clears all 16. */
extern s16 g_world_menu_selection_results[16];
extern s32 g_world_grid_menu_id;
extern s32 (*g_world_menu_aux_callback)(void);
extern s16 g_world_menu_scenario_event_started;
extern world_menu_entry_t* g_world_menu_thread_menu_data;

/* Menu-script callback slots, called as s32 (*)(s32) by the script commands;
 * [2] also carries the name g_world_menu_aux_callback, whose callers invoke
 * it without an argument. */
extern s32 (*g_world_menu_script_callbacks[])(s32);
extern world_menu_palette_source_t* g_world_menu_glyph_sheet; /* decoded glyph page, 0x80 bytes per row */
extern u16 g_world_menu_disabled_number_clut;
extern menu_window_buffer_t g_world_menu_window_buffers[6]; /* 0x8019aca0 */

/*
 * WORLD.BIN routines shared by the world, wldcore, and menu-overlay
 * reconstructions.  Each module's linker table maps these names to its own
 * copy of the routine.
 */
void world_menu_unit_status_banner_thread(void);
void world_menu_equipment_panel_thread(void);
void world_menu_ability_panel_thread(void);

void world_menu_display_hovered_unit_stats(
    RECT* frame_rect, s32 mode, world_menu_number_entry_t* entries, world_unit_status_record_t* status);

void world_menu_build_ability_preview_at_list(void);
void world_menu_scrolling_list_thread(void);
void world_menu_select_unit_action_slots_thread(void);
s32 world_menu_resolve_selection(void);
void world_menu_number_entry_thread(void);
void world_menu_equipment_and_ability_panel_thread(void);
s16 world_menu_step_cursor_wrapping(u16, u8, s32);
void world_menu_run_script_with_callback_suppressed(void);
void world_menu_open_name_entry_key_list(void);
void world_clear_menu_render_buffer(u8* dst, u32 count);
void world_menu_zoom_cursor_frame(const void* source, SPRT* sprite, s32 step);
void world_draw_menu_text_columns_narrow(world_menu_entry_t* entry, s32* row_offset, void* buffer);
void world_menu_init_panel_fade_tiles(void);
void world_draw_menu_number_glyphs(s32 value, s32 digits, void* resource, world_glyph_blit_t* pos);
void world_menu_labeled_number_entry_thread(void);
world_menu_window_command_t* world_menu_parse_draw_window_frame(world_menu_window_command_t* command);
void* world_menu_alloc_window_buffer_pair(void);
void world_menu_animate_window_quad_crop(menu_window_buffer_t* buffers, RECT* rect);
void world_menu_animate_window_quad_crop_full(menu_window_buffer_t* buffers, RECT* rect);
void world_menu_animate_window_quads_to_rects(menu_window_buffer_t* buffers, RECT* to_main, RECT* to_icon);
void world_menu_build_ability_list(s32 mode);
void world_menu_build_and_upload_window_image(s32 width, s32 height, RECT* rect, s32 mode, s32 tail_offset);
void world_menu_build_idle_action_stub(void);
void world_menu_build_sprite_page(s32 page_index, void* base_screen);
void world_menu_check_action_slot_restrictions(s32 menu, s32 value, s32 check_unit);
void world_menu_clear_matching_state_entry(void);

s32 world_menu_dispatch_system_function(
    s32 command, s32 option, s32 unit_id, s32 parameter, s32 enabled, struct battle_ai_command_action* action);

void world_menu_draw_active_window_frames(void);

void world_menu_draw_numeric_display_entries(
    s32 buffer, world_menu_number_entry_t* entries, world_glyph_blit_t* position, s32 count);

void world_menu_draw_pressable_button(s16 index, s32 x, s32 y, s16 pressed, u16 disabled, u16 sound_effect_id);
void world_menu_draw_text_columns(world_menu_entry_t* entry, s32* row_offset, void* buffer);
s32 world_menu_get_display_value_1(void);
s32 world_menu_get_display_value_2(void);
s32 world_menu_get_party_unit_name_id(void);
u8* world_menu_get_script(s32 index);
s32 world_menu_get_value_1(void);
s32 world_menu_get_value_2(void);
void world_menu_init_icon_slot(RECT* rect, s32 width, s32 height, world_texture_prim_t* slot, s32 icon_index);
s32 world_menu_is_busy(s32 unused);
void world_menu_open_entry_window(s32 index, s32 unused_x, s32 unused_y);
u8* world_menu_redraw_text_page_on_scroll(world_menu_entry_t* entry, s32* row_offset, s32* redraw);
u8* world_menu_redraw_text_page_on_scroll_2(world_menu_entry_t* entry, s32* row_offset, s32* redraw);

u8* world_menu_redraw_text_page_on_scroll_in_rect(
    world_menu_entry_t* entry, s32* row_offset, s32* redraw, s32* unused, RECT* area);

s32 world_menu_refresh_learn_skillset_empty(s32 index);
void world_menu_refresh_state_from_script_variables(s32* state);
void world_menu_render_text_image_at_fixed_origin(world_menu_text_image_t* record);
void world_menu_render_text_image_at_record_origin(world_menu_text_image_t* record);
void world_menu_run_companion_overlay_9_10_thread(void);
void world_menu_run_dead_unit_panel_a_2(void);
void world_menu_run_order_unit(void);
s32 world_menu_run_unit_status_banner_countdown(s32 message_id, s32 frames);
u8* world_menu_script_draw_sprite(u8* cmd);
void world_menu_scroll_list_page(s32 dir, u8* menu);
void world_menu_select_icon_cluts(world_menu_icon_sprites_t* menu);
void world_menu_select_task_icon_tile(world_texture_prim_t* slot);
void world_menu_add_gradient_line_primitive(s16* endpoints, u8* rgb, s32 semi_trans, s32 ot_index);
void* world_menu_alloc_buffer(s32 size);
void* world_menu_alloc_ui_buffer(s32 size);
void world_menu_announce_entry_value_thread(void);
void* world_menu_build_and_upload_window_frame_image(s32 width, s32 height, RECT* rect, s32 mode);
void world_menu_add_tile_primitive(RECT* rect, u8* data, u8 semi_trans, s32 priority);
void world_menu_build_icon_record(RECT* rect, world_menu_icon_thread_param_t* param, world_menu_icon_record_t* buffer);
void world_menu_submit_icon_primitives(world_menu_icon_sprites_t* prims);
void world_menu_build_layout_sprites(world_menu_sprite_layout_t* layout, SPRT* sprite);
void world_menu_build_line_box(RECT* rect, world_menu_palette_primitives_t* menu);
s32 world_menu_build_quad_pieces(s32 group, POLY_GT4* poly);
void world_menu_build_skillset_entries(void);
void world_menu_build_unit_status_list(void);
s32 world_menu_build_window_frame_image(s32 width, s32 height, u16* destination);
void* world_menu_build_window_image(s32 width, s32 height, RECT* rect, s32 d, s32 e, s32 f);
void world_menu_cancel_thread_group(world_menu_cancel_context_t* context);
void world_menu_check_action_restrictions(void);
s32 world_menu_check_thread_completion(s32* input);
void world_menu_confirm_action_silently(s32 parameter);
void world_menu_handle_entry_confirm(world_menu_entry_t* entry, s32 row_index);
void world_menu_clear_cursor_positions(void);
void world_menu_clear_entry_flags(void);
void world_menu_clear_transition_active_flag(void);
void world_menu_clear_window_buffer_pointers(void);

void world_menu_configure_status_panel_primitive_colors(
    battle_menu_status_panel_primitives_t* primitives, const battle_menu_status_panel_frame_config_t* state);

void world_menu_display_text(s32 text_id, void* pixels, world_text_draw_origin_t* origin, u8* text);
void world_menu_display_triangle_selection(void);
void world_menu_draw_animated_cursor(world_menu_point_t* position, u16* state, s32 mode);
void world_menu_draw_animated_vertical_cursor(world_menu_point_t* position, u16* state, s32 mode);
void world_menu_draw_row_window_and_cursor(s32 wide, s32 row, s32 mode);
void world_menu_draw_text_columns_2(world_menu_entry_t* entry, s32* row_offset, void* buffer);
void world_menu_draw_thread_status_indicators(void);
void world_menu_enable_all_order_entries(void);
void world_menu_fade_out_thread(void);
void world_menu_fill_pattern_row(u16* source, u16* dest, s32 count, s32 offset);
void world_menu_fill_pattern_row_2(u16* source, u16* dest, s32 count, s32 offset);
void world_menu_free_high_overlay(void);
void world_menu_free_memory(void* buffer);
s32 world_menu_get_entry_item_count(s32 entry_index);
s32 world_menu_get_entry_item_count_2(s32 entry_index);
world_item_icon_source_t* world_menu_get_entry_item_icon_source(s32 index);
s32 world_menu_get_event_state_flag(void);
s32 world_menu_get_item_half_price_display_value(s32 entry_index);
s32 world_menu_get_item_price_display_value(s32 entry_index);
u16 world_menu_get_learn_job_jp(s32 index);
s32 world_menu_get_learn_job_level(s32 index);
s8 world_menu_get_window_scale_step(void);
void world_menu_copy_unit_data_to_status_billboard(struct battle_stats* unit, world_unit_status_billboard_t* output);
void world_menu_init_quad_from_record(u16* source, u16* record, POLY_FT4* poly);
void world_menu_retry_alloc_with_message(u32 size);
void world_menu_run_display_script(u8* script, s32 value);
CVECTOR* world_menu_get_sprite_color(void);
s32 world_menu_handle_remove_all_equipment(void);
void world_menu_handle_window_command_with_scaled_clip(world_menu_window_command_t* command);
s32 world_menu_has_items(s32 mode);
void world_menu_icon_linked_entry_thread(void);
void world_menu_icon_list_thread(void);
void world_menu_init_and_load_scrollable_list(const s16* values, s32 selected_index, u32 state, void* menu);
void world_menu_init_at_list(world_menu_entry_t* entry);
void world_menu_init_palette_and_text(s32 unused);
void world_menu_init_primitive_colors_palette_bank_0(world_menu_palette_primitives_t*);
void world_menu_init_primitive_colors_palette_bank_1(world_menu_palette_primitives_t*);
void world_menu_init_quad(POLY_FT4* poly);
void world_menu_init_record(world_gfx_texture_window_record_t* record);
void world_menu_init_scrollable_list(const s16* values, s32 value, s32 selected, u32 state);
void world_menu_init_scrollable_list_core(const s16* values, s32 selected_index, u32 state);
void world_menu_init_sprite_array(SPRT* sprites, s32 count, s32 clut);
void world_menu_init_subsystems(void);
s32 world_menu_is_input_allowed(void);
s32 world_menu_is_learn_ability_learned(s32 index);
s32 world_menu_lerp_fixed12(s32 from, s32 to, s32 t);
s32 world_menu_lerp_fixed12_b(s32 from, s32 to, s32 t);
void world_menu_load_common_graphics(s32 load_extra_pages);
void world_menu_load_scrollable_list_layout(world_menu_list_record_t* record);
void world_menu_load_text_1bd8(void);
void world_menu_load_text_1bd8_with_face(void);
void world_menu_load_text_1c18(void);
void world_menu_narrow_halfwords_to_byte_record(s32 index, s16* source);
void world_menu_open_scrollable_list(u8* list);
world_menu_window_command_t* world_menu_parse_draw_tiled_rectangle(world_menu_window_command_t* command);
void world_menu_refresh_learn_job_total_jp(s32 index);
void world_menu_release_window_buffer_pair(menu_window_buffer_t* value);
void world_menu_reset_runtime(void);
void world_menu_reset_selection_results(void);
void world_menu_reset_state_arrays(void);
void world_menu_reset_unit_records(void);
void world_menu_run_ability(void);
s32 world_menu_run_best_fit_equipment(void);
void world_menu_run_bunit_transition_thread(void);
void world_menu_run_companion_overlay_7_thread(void);
void world_menu_run_icon_selection_loop(void);
s32 world_menu_run_learn(void);
s32 world_menu_run_learn_job_list(void);
void world_menu_run_main_mode(s32 mode);
void world_menu_run_main_mode_0_thread(void);
void world_menu_run_numeric_display_panel_thread(void);
void world_menu_run_numeric_editor_thread(void);
s32 world_menu_run_remove_ability(void);
s32 world_menu_run_remove_item(void);
void world_menu_run_script_with_palette_mode(void* output, s32 input, s32 mode);
void world_menu_run_skillset_thread(void);
void world_menu_run_system_function_thread(void);
s32 world_menu_run_thread(s32 thread_id, void* menu);
void world_menu_run_unit_debug_editor_thread(void);
u8* world_menu_script_add_blend_draw_mode(u8* script);
u8* world_menu_script_clear_temporary_draw_area(u8* script);
u8* world_menu_script_set_draw_area(u8* script);
u8* world_menu_script_set_sprite_color(u8* script);
void world_menu_select_primitive_color_palette(world_menu_palette_primitives_t* menu, world_menu_color_input_t* input);
void world_menu_select_stat_gauge_level(s32 x, s32 y, s32 otag_index, s32 value);
void world_menu_select_task_icon_tile_2(SPRT* slot);
void world_menu_set_brightness(s32 red, s32 green, s32 blue);
s16 world_menu_set_cursor_position(u8 index, s16 value);
s16 world_menu_set_cursor_position_2(u8 index, s16 value);
void world_menu_set_default_entry_0_text_values(u32 text_index, u32 value);
void world_menu_set_default_entry_1_text_values(s32 value);
void world_menu_set_draw_priority(s16 priority);
void world_menu_set_palette_colors(const u8* source);
void world_menu_set_palette_mode(s32 mode);
void world_menu_set_sprite_color(u8* src);
void world_menu_set_transition_active_flag(void);
void world_menu_set_window_scale_step(s8 step);
void world_menu_start_at_list_thread(void);
void world_menu_start_dead_unit_threads(void);
void world_menu_start_description_text_thread(world_menu_description_record_t* record);
void world_menu_start_main_mode_2(void);
void world_menu_start_or_poll_thread(s32 thread_id, world_menu_thread_data_t* data);
void world_menu_start_system_function_thread(s32 parameter);
void world_menu_start_thread_group(s32 parameter, s32 location_id, s32 window_x, s32 window_y);
s32 world_menu_step_cursor_with_sound(u16 count, u8 cursor_id, s32 input, u8 sound);
s32 world_menu_step_cursor_with_sound_2(u16 count, u8 index, s32 input, u8 value);
s32 world_menu_step_grid_cursor(s32 columns, s32 rows, s32 last_index, s32 old_index, s32 input);
void world_menu_stop_thread(s32 thread_id);
void world_menu_stop_thread_and_wait(s32 thread_id);
void world_menu_stop_unit_status_banner_thread(s32 thread_id);
void world_menu_store_value_for_stored_unit(void);
void world_menu_switch_item_category(s32 input);
void world_menu_text_entry_wait_thread(void);
void world_menu_thread_group_supervisor_thread(void);
void world_menu_toggle_preview_stats_window(s32 flag);
void world_menu_toggle_change_banner_panel_thread(s32 enable);
void world_menu_toggle_ability_panel_thread(s32 enable);
void world_menu_toggle_numeric_editor_thread(s32 enable);
void world_menu_toggle_comparison_banner_thread(s32 flag);
void world_menu_toggle_stat_preview_panel_thread(s32 enable);
void world_menu_start_equipment_and_ability_panel_thread(s32 enabled);
void world_menu_toggle_unit_status_banner(s32 enable);
void world_menu_tween_window_quads(menu_window_buffer_t* buffers, RECT* to, RECT* to2, s32 table);
s32 world_menu_update_clear_all_abilities_prompt(void);

void world_menu_update_entry_cursor_quads(
    world_menu_entry_t* entry, POLY_FT4* quad, POLY_FT4* shadow, s32 unused, s32 cursor);

void world_menu_update_grid_cursor(s16 alternate);

void world_menu_update_icon_cursor_sprites(
    world_menu_icon_thread_param_t* param, world_menu_icon_sprites_t* record, s32 frame, s32 cursor);

void world_menu_update_panel_fade(void);
void world_menu_update_thread_7_idle_countdown(void);
void world_menu_widen_byte_record_to_halfwords(s32 index, s16* out);
s32 world_menu_widen_bytes_to_halfwords(u16* destination, const u8* source);
void world_menu_window_frame_thread(void);
void world_menu_set_slot_4_cursor_index(s32 index);
s32 world_menu_get_thread_activity(void);
void world_menu_display_text_entry_mode_1(s32 p1, s32 p2, s32 p3);
void world_menu_display_text_entry(s32 text_id, void* image, void* origin);
void world_menu_init_sprite(SPRT* sprite);
void world_menu_set_text_origin(s16 x, s16 y);

/* card */
/* Save-file image held in g_world_load_work_buffer (0x801cd1ec). WORLD's
 * serializer and loader establish the fields below; CARD uses the same
 * layout through g_card_save_buffer_pointer. */
typedef struct world_card_save_buffer {
    u8 _unused_0000[0x100];
    u8 slot;             /* 0x100; save slot, 0xff while the buffer is incomplete */
    u8 name[0x10];       /* 0x101; save-description leader name, 0xfe-terminated */
    u8 name_terminator;  /* 0x111 */
    u8 job_id;           /* 0x112 */
    u8 level;            /* 0x113 */
    u8 month;            /* 0x114; script variable 0x2e */
    u8 day;              /* 0x115; script variable 0x2f */
    u8 location;         /* 0x116; script variable 0x31, location text index */
    u8 format_version;   /* 0x117; CARD writes 4 and WORLD load requires 4 */
    u8 parity_bits[8];   /* 0x118; one parity bit per 128-byte block */
    s32 elapsed_seconds; /* 0x120 */
    /* 0x124-0x1dbe: same layout as CARD's card_save_buffer_t, filled by
     * world_card_build_save_image from the SCUS globals named in its bcopy calls. */
    u8 treasure_acquisition_date_bits[0x35];     /* 0x124; 47 packed 9-bit dates and 1 spare bit */
    u8 land_discovery_date_bits[0x12];           /* 0x159; packed 9-bit discovery dates */
    u8 proposition_last_attempt_date_bits[0x6c]; /* 0x16b; packed 9-bit dates */
    u8 proposition_states[0x60];                 /* 0x1d7; low 6 bits are a remaining-day counter */
    u8 _unused_0237[1];                          /* 0x237; serialized and parity-covered */
    u8 saved_records[5][0x38];                   /* 0x238; domain remains unproven */
    u32 saved_data_bits[40];                     /* 0x350 */
    u32 secondary_saved_data_bits[2];            /* 0x3f0 */
    u8 brave_story_character_ages[0x40];         /* 0x3f8 */
    u8 proposition_count;                        /* 0x438 */
    u8 active_propositions[8][9];                /* 0x439 */
    u8 _unused_0481[3];                          /* 0x481; serialized and parity-covered */
    u8 party_records[20][0xe0];                  /* 0x484; party_data_t records truncated to 0xe0 bytes */
    u8 item_quantities[ITEM_ID_COUNT];           /* 0x1604; g_main_item_quantities */
    u8 poached_item_quantities[ITEM_ID_COUNT];   /* 0x1704; g_main_item_poached_quantities */
    /* 0x1804; first 0x40 bytes are Move-Find-Item flags, second half unknown. */
    u8 item_location_flags[0x80];
    s32 script_variables[0x100];   /* 0x1884 */
    game_options_fields_t options; /* 0x1c84; copied to g_main_game_options by world_card_init_screen */
    u8 _unknown_1c88;              /* 0x1c88 */
    s8 item_type_order_0[0xc];     /* 0x1c89; signed, -1-terminated */
    u8 _unused_1c95[1];            /* 0x1c95; serialized and parity-covered */
    s8 item_type_order_1[8];       /* 0x1c96; signed, -1-terminated */
    u8 _unused_1c9e[1];            /* 0x1c9e; serialized and parity-covered */
    s8 item_type_order_2[7];       /* 0x1c9f; signed, -1-terminated */
    s8 item_type_order_3[5];       /* 0x1ca6; signed, -1-terminated */
    s8 item_type_order_4[5];       /* 0x1cab; signed, -1-terminated */
    s8 item_type_order_5[7];       /* 0x1cb0; signed, -1-terminated */
    s8 item_type_order_6[5];       /* 0x1cb7; signed, -1-terminated */
    u8 weapon_page_order[0x8a];    /* 0x1cbc */
    u8 helmet_page_order[0x1d];    /* 0x1d46 */
    u8 armor_page_order[0x25];     /* 0x1d63 */
    u8 accessory_page_order[0x21]; /* 0x1d88 */
    u8 item_page_order[0x15];      /* 0x1da9 */
    u8 _unused_1dbe[0x42];         /* 0x1dbe; parity-covered tail */
} world_card_save_buffer_t;
typedef char world_save_buffer_options_offset_must_be_0x1c84
    [((unsigned long)&((world_card_save_buffer_t*)0)->options == 0x1c84) ? 1 : -1];
typedef char world_save_buffer_size_must_be_0x1e00[(sizeof(world_card_save_buffer_t) == 0x1e00) ? 1 : -1];

/* Partial card-file header: WORLD 0x8013300c puts byte +3 into the upper
 * half of FileOpen's creation mode. The remaining header is not modeled. */
typedef struct world_card_file_header {
    u8 _unused_00[3];
    u8 allocation_blocks;
} world_card_file_header_t;

extern world_card_save_buffer_t* g_world_load_work_buffer;
extern u8 g_world_card_info_pending;
extern s16 g_world_card_list_scroll_velocity;
extern u8 g_world_card_menu_step;
extern s32 g_world_card_open_descriptor;
extern card_save_icon_t g_world_card_save_icon_records[15];
extern u8 g_world_card_save_title_template[0x16];
extern u8 g_world_card_selected_slot;
extern u8 g_world_card_slot_cursor_row;
extern s16 g_world_card_menu_request_state;
extern void* g_world_card_save_file_names[];

/* Encoded captions addressed directly by world_card_build_save_slot_description,
 * immediately after the 15 save-file pointers at g_world_card_save_file_names. */
extern u8 g_world_card_slot_text_table[];
extern s16 g_world_card_slot_select_timer;
extern RECT g_world_card_screen_background_tile;
extern world_menu_thread_data_t g_world_card_slot_select_menu;
extern u8 g_world_card_slot_select_running;

/* Menu-thread descriptors of the same 0x3c-spaced family as g_world_card_saving_message
 * below; world_card_run_mode_select_step assigns each into g_world_card_pending_message. */
extern world_menu_thread_data_t g_world_card_save_unavailable_message[];
extern world_menu_thread_data_t g_world_card_no_save_data_message[];
extern world_menu_icon_thread_param_t g_world_card_mode_select_menu;
extern u8 g_world_card_mode_select_running;

/* A pointer cell, not a record: cleared to 0, tested, and assigned
 * g_world_card_save_unavailable_message / g_world_card_no_save_data_message by world_card_run_mode_select_step. */
extern world_menu_thread_data_t* g_world_card_pending_message;
extern world_menu_thread_data_t g_world_card_saving_message[];
extern world_menu_thread_data_t g_world_card_loading_message[];
extern world_menu_thread_data_t g_world_card_checking_message[];
extern world_menu_thread_data_t g_world_card_load_error_message[];
extern world_menu_thread_data_t g_world_card_save_error_message[];
extern u8 g_world_card_load_step_state;
extern u8 g_world_card_save_step_state;
extern world_menu_thread_data_t g_world_card_format_prompt[];
extern world_menu_thread_data_t g_world_card_formatting_message[];
extern world_menu_thread_data_t g_world_card_access_error_message[];
extern s8 g_world_card_slot_scan_active;
extern s8 g_world_card_format_delay_counter;
extern s8 g_world_card_error_message_closing;
extern world_menu_window_command_t g_world_card_slot_window_command;
extern world_menu_window_rect_source_t g_world_card_slot_window;
extern u16 g_world_card_slot_description_row_ids[];
extern world_draw_number_command_t g_world_card_slot_number_command;
extern world_draw_number_command_t g_world_card_playtime_hours_command;
extern world_draw_number_command_t g_world_card_playtime_minutes_command;
extern world_draw_number_command_t g_world_card_playtime_seconds_command;
extern world_menu_point_t g_world_card_slot_cursor_point;
extern u16 g_world_card_slot_cursor_anim;
extern u8 g_world_card_slot_list_input_ready;
extern u8 g_world_card_overwrite_prompt_running;
extern u8 g_world_card_load_prompt_running;
extern world_gfx_sprite_desc_t g_world_card_slot_quad_0;
extern world_gfx_sprite_desc_t g_world_card_slot_quad_1;
extern world_gfx_sprite_desc_t g_world_card_slot_quad_2;
extern world_gfx_sprite_desc_t g_world_card_slot_quad_3;
extern world_gfx_sprite_desc_t g_world_card_slot_quad_4;
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
extern s16 g_world_card_active_slot;
extern s16 g_world_card_list_first_visible_row;
extern u8* g_world_card_directory_buffer;
extern u8 g_world_card_selectable_slot_count;
extern u8 g_world_card_save_slot_descriptions[][0x8C];
s32 world_card_build_save_slot_description(s32 mode, u8* slot_graphic);
s32 world_card_read_file_chunked(const char* filename, u8* destination, u32 size);
void world_card_build_save_file_header(s32 slot, s32 level, struct card_save_header* header);
s32 world_card_check_selected(void);
void world_card_clear_events(void);
s32 world_card_close_file_with_retries(s32 descriptor);
void world_card_consume_bios_events(void);
void world_card_consume_hardware_events(void);
s32 world_card_count_free_blocks(struct DIRENTRY* entries, s32 count);
s32 world_card_count_save_files(struct DIRENTRY* dir, s32 all);
s32 world_card_count_selectable_save_slots(void);
s32 world_card_create_new_with_retries(s32 port, s32 retry_limit);
s32 world_card_delete_file(char* name);
void world_card_draw_slot_text(s32 slot);
s32 world_card_format_selected_slot(void);
u8 world_card_get_selected_slot(void);
void world_card_init_menu_state(void);
s32 world_card_load_globals_from_save_image(s32 partial_load);
s32 world_card_load_with_retries(s32 channel, s32 attempts);
s32 world_card_open_file_with_retries(const char* filename, s32 mode);
s32 world_card_poll_bios_events(void);
s32 world_card_poll_hardware_events(void);
s32 world_card_poll_info_with_retries(s32 channel, s32 count);
s32 world_card_probe_selected(void);
s32 world_card_read_file_with_retries(s32 descriptor, void* destination, s32 size);
void world_card_run_load_step(s32 slot);
s32 world_card_run_menu_screen(s32 slot_count);
void world_card_run_mode_select_step(void);
void world_card_run_save_step(s32 slot);
void world_card_run_slot_select_step(void);
void world_card_scroll_menu_list(s32 row);
void world_card_set_selected_slot(u8 slot);
void world_card_update_and_draw_slot_list(s32 input);
void world_card_update_menu_request(void);
void world_card_update_save_slot_playtime(s32 slot);
void world_card_update_slot_scan(void);
s32 world_card_wait_for_bios_event(void);
s32 world_card_wait_for_hardware_event(void);
s32 world_card_wait_for_selected_status(s32 attempts);
s32 world_card_write_file_with_retries(s32 descriptor, const void* source, s32 size);
void world_card_build_save_image(s32 slot);
s32 world_card_seek_file_with_retries(s32 descriptor, s32 offset, s32 origin);
s32 world_card_write_buffer_to_file(const char* filename, const u8* source, u32 size, s32 create_file);

/* item */
/* Halfword list entries retain their high byte when filtered at 0x801220d8;
 * only the low byte selects an item. The complete halfword sentinel is -1. */
typedef union world_item_list_entry {
    s16 value;
    struct {
        u8 item_id;
        u8 _unused_01;
    } bytes;
} world_item_list_entry_t;
typedef char world_item_list_entry_size_must_be_2[(sizeof(world_item_list_entry_t) == 2) ? 1 : -1];

extern world_oriented_sprite_t g_world_item_category_tab_cursor_sprite;
extern struct world_item_stat_detail g_world_item_preview_stat_detail;
extern u8* g_world_item_sorted_lists[];
extern world_image_location_t g_world_item_icon_image_location;
extern s16 g_world_item_icon_clut_base_x; /* item icon CLUT base x */
extern s16 g_world_item_icon_clut_base_y; /* item icon CLUT base y */

/* Menu-subflow "running" flags. The readers sign-extend (lb), so these are
 * signed; the initialiser only stores 0/1/2 and cannot distinguish. */
extern s8 g_world_item_menu_initialized;
extern s8 g_world_item_best_remove_initialized;
extern u8 g_world_item_submenu_initialized;
extern u8 g_world_item_category_menu_initialized;
extern s8 g_world_item_rearrange_initialized;
extern u8 g_world_item_action_menu_initialized;
extern u8 g_world_item_sort_order_initialized;
extern s8 g_world_item_equipping_units_initialized;
extern world_menu_entry_t g_world_item_icon_menu;
extern u8 g_world_item_menu_unit_index;
extern u8 g_world_item_submenu_exit_pending;
extern s8 g_world_remove_item_unit_index;
extern s16 g_world_remove_item_saved_primary_input;
extern s16 g_world_remove_item_saved_secondary_input;
extern u16 g_world_remove_item_saved_newly_pressed;
extern world_menu_number_range_t g_world_item_action_quantity_range;
extern s16 g_world_item_action_option_states[3];
extern world_menu_icon_thread_param_t g_world_item_action_menu;
extern u16 g_world_remove_item_cursor_anim;
extern world_menu_point_t g_world_remove_item_cursor_point;
extern world_menu_icon_thread_param_t g_world_item_quantity_change_menu;
extern world_menu_icon_thread_param_t g_world_item_unequip_all_menu;
extern u8 g_world_item_list_window_script[];
extern u8 g_world_item_hp_mp_bonus_window_script[];
extern s8 g_world_item_menu_mode; /* item menu mode; -1 closes */

/* The two item-category icon strips; the active one is kept in g_world_item_category_icon_strip. */
extern world_menu_icon_thread_param_t g_world_item_category_icon_strip_a;
extern world_menu_icon_thread_param_t g_world_item_category_icon_strip_b;
extern s16 g_world_item_category_tabs_script;       /* window descriptor (narrow variant) */
extern s16 g_world_item_category_tabs_wide_script;  /* window descriptor (wide variant) */
extern s16 g_world_item_category_tab_sprites[][10]; /* 20-byte rects, one per row */
extern u8 g_world_item_category_tab_x[];            /* per-row base x */
extern u8 g_world_item_category_input_lock;
extern world_item_icon_source_t g_world_item_type_icon_source;
extern u8 g_world_item_menu_saved_browse_enabled;
extern s8 g_world_remove_item_previewed_slot;
extern world_menu_halfword_view_t g_world_remove_item_equipment[5];
extern s8 g_world_item_category_previewed_cursor;
extern u16 g_world_item_rearrange_picked_cursor;   /* cursor position of the picked-up item */
extern u16 g_world_item_rearrange_picked_scroll;   /* scroll offset when picked up */
extern u16 g_world_item_rearrange_preview_cursor;  /* last previewed cursor position */
extern u8 g_world_item_rearrange_anim_frame;       /* animation frame counter */
extern s16 g_world_item_category_strip_text_ids[]; /* option text ids of the active strip, -1 terminated */
extern world_menu_icon_thread_param_t* g_world_item_category_icon_strip;
extern u16 g_world_item_saved_selected_unit_index;
extern u8 g_world_item_menu_category;
extern u8 g_world_item_category_view;
void world_build_item_icon_sprite(SPRT* sprite, s32 item_id);
s32 world_count_item_equipped_by_party(s32 item_id);
s32 world_item_build_category_list(s16 unit_id, u16 sort_mode, s8 category, world_item_list_entry_t* entries, u8 mode);
void world_item_build_icon_source(world_item_icon_source_t* source, s32 item_id);
void world_item_build_status_list_polygons(s16 item_id);
world_item_icon_source_t* world_item_build_type_icon_source(s32 entry_index);

void world_item_calculate_equipment_swap_stat_delta(
    world_item_stat_detail_t* detail_total, world_item_stat_summary_t* total, u16* items_before, u16* items_after);

void world_item_calculate_swap_stat_delta(
    world_item_stat_detail_t* output, world_item_stat_summary_t* delta, s16 item_before, s16 item_after, s32 slot);

void world_item_cash_out_excess_inventory(void);
s32 world_item_change_quantity_on_equip(s32 item_id, s32 delta);

void world_item_combine_stat_details(world_item_stat_detail_t* output, world_item_stat_detail_t* before,
    world_item_stat_detail_t* after, s32 multiplier);

s32 world_item_check_two_hands_for_weapons(struct weapon_pair* slots, s32 two_hands_support);
s32 world_item_count_owned_and_equipped(s32 item_id);
s32 world_item_count_selected_equipped_by_party(void);
s32 world_item_count_selected_owned_and_equipped(void);
void world_item_draw_weapon_hand_icons(s16 weapon_id);
s32 world_item_filter_sorted_entries(s32 list_index, world_item_list_entry_t* entries);
void world_item_finalize_sorted_list(s32 list_index, s16* sorted_ids);
s32 world_item_get_equip_candidate_brightness(s32 unit_index);
s32 world_item_get_equip_candidate_marker(s32 unit_index);

/* WORLD menu helpers. Half-price is clamped to one; stock to a byte. */
s32 world_item_get_half_price(s32 item_id);
s32 world_item_get_price(s32 item_id);
u8 world_item_get_ranking_value(s32 item_id);
s32 world_item_get_selected_half_price(void);
u8 world_item_get_type(s32 item_id);
void world_item_get_type_icon_rect(s32 type, world_item_icon_source_t* out);
item_menu_category_e world_item_get_menu_category(s32 item_id);
s32 world_item_is_in_sorted_list(s32 item_id, s32 list_index);

void world_item_populate_stat_preview(
    s32 g_main_item_item_flags, world_item_stat_summary_t* summary, world_item_stat_detail_t* detail, s32 slot);

void world_item_prepend_sorted(s32 item_id, s32 list_index);
void world_item_reconcile_sorted_list(s32 list_index);
void world_item_remove_sorted(s32 item_id, s32 list_index);
s8 world_item_run_action_menu(void);
s32 world_item_run_category_menu(void);
s32 world_item_run_rearrange_mode(void);
s8 world_item_run_sort_order_menu(void);
world_unit_equipment_slot_e world_item_select_equipment_slot(s16 unit_index, s32 item_id);
s32 world_item_show_units_equipping_selected(void);
void world_item_sort_id_list(s32 mode, world_item_list_entry_t* list);
void world_item_sum_equipment_stat_details(struct world_item_stat_detail* total, u16* equipment);

/* shop */
/* Two-byte shop availability mask, stored most significant byte first. */
typedef struct world_shop_item_availability {
    u8 high;
    u8 low;
} world_shop_item_availability_t;

extern s16 g_world_shop_fitting_room_items[][5];
extern u8 g_world_shop_hire_menu_initialized;
extern u8 g_world_shop_item_category;
extern world_shop_item_availability_t g_world_shop_item_availability[];
extern s16 g_world_shop_menu_step;
extern s16 g_world_shop_quantity_limit;
extern world_menu_thread_data_t g_world_shop_quantity_menu_data;
extern u8 g_world_shop_background_visible;
extern world_menu_icon_thread_param_t g_world_shop_main_menu_thread_data;
extern world_menu_icon_thread_param_t g_world_shop_fur_menu_thread_data;

/* The cursor doubles as the soldier-office mode (world_shop_get_soldier_office_entry_fee). */
extern world_menu_icon_thread_param_t g_world_shop_hire_menu_thread_data;
extern RECT g_world_shop_background_rect;
extern RECT g_world_shop_backdrop_tile_rect;
extern RECT g_world_shop_edge_tile_rect;
extern RECT g_world_shop_background_vram_rect;
extern RECT g_world_shop_vram_backup_rect;
extern u8 g_world_shop_main_menu_running;     /* menu open flag */
extern u8 g_world_shop_exit_fade_started;     /* fade-out started */
extern u8 g_world_shop_exit_step_initialized; /* state initialised */
extern u8 g_world_shop_obtain_gil_menu_script[];
extern s16 g_world_shop_purchase_menu_script[];
extern s8 g_world_shop_purchase_list_initialized;
extern s8 g_world_shop_purchase_equip_check_pending;
extern s16 g_world_shop_purchase_message_wait;
extern u8 g_world_shop_quantity_menu_running;
extern u8 g_world_shop_equip_candidate_initialized;
extern u8 g_world_shop_equip_candidate_preview_visible;
extern u8 g_world_shop_sell_list_menu_script[];
extern s8 g_world_shop_sell_list_initialized;
extern s16 g_world_shop_sell_message_wait;
extern u8 g_world_shop_sell_result; /* sell-menu thread result */
extern u8 g_world_shop_fitting_room_initialized;

/* Two-entry menu chain: row 3 of [0] opens [1] through [0]'s parent-index
 * table at 0x80195518. */
extern world_menu_icon_thread_param_t g_world_shop_fitting_room_menu_thread_data[2];
extern u8 g_world_shop_fitting_item_list_menu_script[];
extern s8 g_world_shop_fitting_equip_menu_initialized;
extern s8 g_world_shop_fitting_equip_slot;
extern s8 g_world_shop_fitting_item_list_close;
extern u16 g_world_shop_fitting_slot_cursor_anim;
extern world_menu_point_t g_world_shop_fitting_slot_cursor_point;
extern u8 g_world_shop_fitting_cost_menu_script[];
extern s8 g_world_shop_equip_slot_preview_initialized;
extern u16 g_world_shop_equip_slot_cursor_anim;
extern world_menu_point_t g_world_shop_equip_slot_cursor_point;
extern world_menu_entry_t g_world_shop_checkout_confirm_menu;
extern world_menu_entry_t g_world_shop_checkout_short_gil_menu;
extern u8 g_world_shop_checkout_prompt_initialized;

/* The best-fit confirmation menu entry, followed directly by its open/closed
 * gate byte, as g_world_shop_sell_equipped_menu / _open are paired. */
extern world_menu_entry_t g_world_shop_best_fit_menu;
extern s8 g_world_shop_best_fit_menu_open;
extern world_menu_entry_t g_world_shop_sell_equipped_menu;
extern u8 g_world_shop_sell_equipped_menu_open;
extern u16 g_world_shop_sell_equipped_cursor_anim;
extern world_menu_point_t g_world_shop_sell_equipped_cursor_point;
extern u8 g_world_shop_sell_subflow_initialized;
extern u8 g_world_shop_fur_opened; /* fur shop opened */
extern s16 g_world_shop_sale_confirm_menu_script[];
extern s8 g_world_shop_fur_sell_list_initialized;
extern s16 g_world_shop_fur_sale_message_wait;
extern u8 g_world_shop_fur_quantity_menu_running; /* sell-menu thread result */
extern s16 g_world_shop_fur_purchase_menu_script[];
extern s8 g_world_shop_fur_purchase_list_initialized;
extern s8 g_world_shop_fur_equip_check_pending;
extern s16 g_world_shop_fur_purchase_message_wait;
extern u8 g_world_shop_repurchase_result; /* repurchase-menu thread result */
extern world_menu_icon_thread_param_t g_world_shop_rename_confirm_menu;
extern u8 g_world_shop_recruit_preview_initialized;
extern u8 g_world_shop_rename_entry_initialized;
extern u8 g_world_shop_rename_message_wait;
extern u8 g_world_shop_rename_prompt_initialized;
extern u8 g_world_shop_rename_entry_2_initialized;
extern u8 g_world_shop_soldier_office_fee_menu_script[];
extern u8 g_world_shop_roster_count_menu_script[];
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
 * ATTACK overlay has an unrelated halfword at this same address. */
extern s8 g_world_shop_previewed_equip_slot;
extern s16 g_world_shop_previewed_unit_index;
extern u8 g_world_shop_checkout_complete;
extern u8 g_world_shop_checkout_insufficient_gil;
extern u8 g_world_shop_checkout_prompt_active;
extern u8 g_world_shop_checkout_prompt_result;
extern world_menu_entry_t* g_world_shop_checkout_prompt_menu;
extern u8 g_world_shop_sell_equipped_prompt_active;
extern u8 g_world_shop_sell_equipped_slot;
extern u8 g_world_shop_sell_equipped_result_shown;
extern s16 g_world_shop_sell_subflow_step;
extern u8 g_world_shop_fur_waiting_message;      /* waiting for message dismissal */
extern u8 g_world_shop_fur_has_secret_hunt;      /* any unit has Secret Hunt */
extern u8 g_world_shop_fur_has_poached_stock;    /* any poached item in stock */
extern u16 g_world_shop_sell_quantity_item;      /* item being sold */
extern u8 g_world_shop_sell_total_shown;         /* confirmation prompt shown */
extern s32 g_world_shop_repurchase_price;        /* buy-back unit price */
extern u16 g_world_shop_repurchase_item;         /* item being bought back */
extern u8 g_world_shop_repurchase_confirm_shown; /* confirmation prompt shown */
extern s8 g_world_shop_fur_access_granted;
extern u8 g_world_shop_hire_result_shown;
extern u8 g_world_shop_rename_error_shown;
extern u8 g_world_shop_party_unit_count;
extern const u8* g_world_shop_entered_unit_name;
extern u32 g_world_shop_service_fee;
extern s16 g_world_shop_id;
extern u8 g_world_shop_cost_window_visible;
void world_shop_run_item_sell_list_step(void);
void world_shop_run_item_sell_confirm_step(void);
s32 world_shop_rename_unit_get_unit_marker(void);
s32 world_shop_add_fitting_room_cost(s32 delta);
s32 world_shop_adjust_poached_item_quantity(s32 item_id, s32 delta);
void world_shop_apply_fitting_room_items_to_all_units(void);
void world_shop_build_fitting_room_best_fit_equipment(s16 unit_index, s16 shop_id, s16* out);
s32 world_shop_build_item_list(s16 unit_id, s16 shop_id, s16 category, world_item_list_entry_t* entries, s32 mode);
void world_shop_buy_from_fitting_room(void);
void world_shop_finalize_unit_equips_from_fitting_room(s16 formation_index, s16 equipment_index);
s32 world_shop_fitting_room_get_unit_marker(void);
s32 world_shop_get_gil_minus_fitting_room_cost(void);
s32 world_shop_get_selected_poached_item_quantity(void);
void world_shop_install_callbacks_and_run(void);
s32 world_shop_obtain_gil(s32 delta);
s32 world_shop_get_soldier_office_entry_fee(void);
void world_shop_return_unit_equip_to_fitting_room(s16 unit_index, s16 equipment_index);
void world_shop_run_equip_candidate_step(void);
s32 world_shop_run_fitting_room_checkout_prompt(void);
s32 world_shop_run_item_purchase_quantity_menu(void);
s32 world_shop_run_item_repurchase_menu(void);
s32 world_shop_run_item_sell_menu(void);
s32 world_shop_run_item_sell_menu_2(void);
void world_shop_run_obtain_gil_menu(void);
void world_shop_run_soldier_office_fee_menu_script(void);
void world_shop_update_unit_selection(void);
void world_shop_run_screen(s32 shop_type);

/* other */
extern s16 g_world_field_object_use_request;
extern u16 g_world_field_object_wait_status;
extern s16 g_world_frame_arg;
extern s32 g_world_frame_counter;
extern s8 g_world_cursor_bob_thresholds[];        /* (threshold, value) pairs, 0-terminated; entry 0 is the modulus */
extern s8 g_world_cursor_bob_active_thresholds[]; /* (threshold, value) pairs, 0-terminated */
extern u16 g_world_saved_animation_speed;         /* saved animation speed */
extern u16 g_world_animation_speed_forced;        /* animation speed override active */
extern u8 g_world_rotation_speed_frames[];
extern u8 g_world_location_names_resident[];
extern s16 g_world_debug_variable_list_request_state;
extern world_menu_quad_page_t world_D_80173CBC[2];
extern s32 g_world_preview_stats_window_active;
extern u8 g_world_dismiss_unit_prompt_running;
extern world_menu_icon_thread_param_t g_world_best_fit_confirm_menu;
extern u8 g_world_order_unit_menu[];
extern s8* g_world_sort_key_lists[];
extern u8 g_world_best_fit_slot_categories[5];

/* Second slot-category table of world_shop_build_fitting_room_best_fit_equipment. */
extern u8 g_world_fitting_room_best_fit_slot_categories[5];
extern s32 g_world_saved_game_options;
extern s32 g_world_finish_operation_event_id;
extern u8 g_world_order_unit_saved_browse_enabled;
extern u8 g_world_action_menu_skillsets[];
extern u16 g_world_frame_result_override;
extern s32 g_world_fixed_math_overflow; /* set by world_cross_product_q12 on Q12 overflow */
s32 world_cross_product_q12(s32 a, s32 b, s32 c, s32 d);
s32 get_current_facing_byte_from_misc_id(s32 misc_id);
s32 world_coords_fit_in_byte(s32 x, s32 y);
void world_fill_16_bytes_fe(u8* buf);
void world_free_work_buffer(void);
u32 world_ps_sort_sprite_bg(u32* tag, GsOT* ot, s32 z, s32 len);
s32 world_get_known_skillsets(s16 unit_id, s16* skillsets);
s32 world_list_contains_value(s32 value);
s16 world_move_menu_cursor_horizontal(u16 count, u8 index, s32 buttons);
void world_process_inflict_status_commands(void);
void world_rotate_unit(const u8*);
void world_block_start_thread(void);

/* unnamed */
extern s8 D_8018D18A;
extern s32 D_801A668C;
extern s8 D_801C833C;
extern u8 D_801CC7D0;
extern u8 D_801CC7D8;
extern u8 D_801CD0EC;
extern u8 D_801CD0F4;
extern s32 D_801CD7DC;
extern s32 D_801CD83C;
extern s32 D_801CD854;

#endif
