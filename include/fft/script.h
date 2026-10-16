#ifndef SCRIPT_H
#define SCRIPT_H

/* The event script interpreter for BATTLE and WORLD: its state, the event loader, the instruction handlers and the
 * requests they leave for the frame loop. The bytecode is in script_instruction.h and the script variables in
 * script_variable.h. */

#include "fft/gfx.h"
#include "fft/map.h"
#include "fft/menu.h"
#include "fft/menu_command.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/script_instruction.h"
#include "fft/unit_event.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* overlay workspace */
/* Provisional work block at *g_ai_workspace_ptr: the menus build their
 * ability list and the outermost scrolling list's packet pages in it, events
 * keep the staged-status snapshot at 0x5f0. */
typedef struct event_work {
    menu_ability_list_t ability_list;     /* 0x000 */
    u8 _padding_386[2];                   /* alignment */
    menu_list_page_t list_pages[2];       /* 0x388 */
    unit_status_staging_t status_staging; /* 0x5f0 */
} event_work_t;
typedef char event_work_size_must_be_2532[(sizeof(event_work_t) == 2532) ? 1 : -1];

/* event loading */
/* The EVENT/TEST.EVT block of the loaded event. */
extern event_file_block_t* g_script_event_block;

/* Operand-byte counts per opcode, excluding the opcode byte; not per-operand
 * width schemas. The table ends after opcode 0xf3. */
extern u8 g_script_event_instruction_sizes[0xf4];
extern s32 g_script_loaded_event_id;
extern s32 g_script_next_event_id;

void script_load_event(s32 id);
s32 script_load_next_event(void);
void script_reset_event_state(void);
s32 script_get_current_scenario_finish_operation(void);
u32 script_get_event_finish_operation(void);

/* interpreter */
extern s32 g_script_event_speed;
extern u16 g_script_frame_result_override;
extern s16 g_script_scenario_event_active;
extern s16 g_script_threads_idle;
extern u8 g_event_thread_status_snapshot[]; /* per-thread running flags sampled before the yield, indexed 1..15 */

void script_run_next_event(void);
void script_run_next_event_unused(void);
void script_start_current_event_thread(void);
void script_opcode_2a_block_start_thread(void);
/* Empty in WORLD, which links its own stub under this name. */
void script_opcode_49_add_unit_start_thread(void);
void script_opcode_45_add_unit(void);
/* BATTLE and WORLD each have their own body. */
void script_execute_event(void);
s32 script_run_event_frame(u32* ot, u32 buttons);
void script_frame_after_requests_stub(void);
void script_frame_after_threads_stub(void);
void script_set_event_speed(s32 speed);
s32 script_is_deployment_running(u32* ot, u32 buttons);
s32 script_is_tutorial_event_slot(void);
void script_opcode_74_unknown(const u8* unused_parameters);

/* attack graphics preload */
/* Whether an event instruction loaded the ATTACK overlay and its graphics
 * before the deployment screen starts. */
typedef enum script_attack_entry_mode {
    SCRIPT_ATTACK_ENTRY_NORMAL = 1,       /* set when an event thread starts */
    SCRIPT_ATTACK_ENTRY_PRELOADED = 2,    /* script_opcode_82_load_attack_graphics ran */
    SCRIPT_ATTACK_ENTRY_PRELOAD_USED = 3, /* menu_init_attack_resources_and_threads skipped its loads */
} script_attack_entry_mode_e;

extern u16 g_script_attack_entry_mode; /* script_attack_entry_mode_e */

void script_opcode_82_load_attack_graphics(void);

/* instruction scanning */
s32 script_find_instruction_byte_offset(s32 offset, s32 opcode);
/* Returns the offset after the matching target marker; a missing target stops
 * the current thread. alternate_opcode accepts -1. */
s32 script_find_jump_target(s32 limit_offset, event_opcode_e target_opcode, s32 alternate_opcode, s32 target_id);
/* Reads an unaligned little-endian signed 16-bit operand. */
s16 script_read_s16(const void* source);

/* scenario conditions */
extern u8 g_script_condition_param_lengths[];
extern s32 g_script_condition_unit_battle_id;

s32 script_run_scenario_conditions(void);

/* values */
extern s32 g_script_rand16_state;

void script_seed_random_from_vsync(void);
void script_clamp_s32_to_range(s32* value, s32 minimum, s32 maximum);
s32 script_scale_music_volume(s32 volume);

/* calendar */
void script_opcode_85_unlock_date(u32* bitset, s32 date_index, s32 month, s32 day);
void script_advance_date_by_one_year(void);

/* units */
/* Four unit bindings used by the scenario's slot selectors. */
extern s16 g_script_unit_slots[EVENT_UNIT_SELECTOR_SLOT_COUNT];

/* Event-unit lookup uses the resident scene's interpreter state. */
s32 script_resolve_runtime_id(s32 unit_id);
void script_join_units_silently(void);
void script_opcode_7a_dismiss_unit(s32 sprite_set);
void script_opcode_99_blue_remove_unit(s32 unit_id);
void script_blue_remove_all_enemy_units(void);
void script_opcode_83_change_stats(const event_change_stats_parameters_t* parameters);
void script_inflict_status_thread(void);
void script_process_inflict_status_commands(void);
void status_queue_current_status_graphics(s32 battle_id, s32 enabled);

/* unit movement */
extern map_tile_coordinates_t g_script_warp_coords;

/* Task words of a sprite-move thread. The SpriteMove instruction and the
 * thread itself store the moving unit's runtime id; WaitSpriteMove matches it. */
enum {
    SPRITE_MOVE_THREAD_WORD_RUNTIME_ID = 0,
};

void script_opcode_28_walk_to(const event_walk_to_parameters_t* parameters);
void script_opcode_70_jump(const event_jump_parameters_t* parameters);
void script_opcode_80_march(const event_march_parameters_t* parameters);
void script_warp_unit(const event_warp_unit_parameters_t* instruction);
void script_warp_unit_display_to_paired_unit(s32 runtime_id);
void script_opcode_98_teleport_in(s32 unit_id, s32 unused);
void script_opcode_94_teleport_out(s32 unit_id, s32 remove);
void script_sprite_move(void);
void script_sprite_move_beta(void);

/* unit facing and animation */
extern u16 g_script_unit_face_slot_order[];

void script_unit_anim(const event_unit_anim_parameters_t* parameters);
void script_opcode_8c_unit_anim_rotate(const event_unit_anim_rotate_parameters_t* parameters);
void script_face_unit(const event_face_unit_parameters_t* parameters, s32 second_only);
void script_opcode_69_face_tile(const event_face_tile_parameters_t* parameters);
void script_rotate_unit(const event_rotate_unit_parameters_t* parameters);

/* unit appearance */
void script_opcode_97_reset_palette(const event_unit_parameters_t* parameters);
void script_opcode_68_mirror_sprite(const event_mirror_sprite_parameters_t* parameters);
void script_opcode_32_color_unit(const event_color_unit_parameters_t* parameters);
void script_set_units_palette_update_suppression(s32 event_unit_id, s32 enable);
void script_opcode_81_set_unit_movement_effect_suppression(const event_movement_effect_parameters_t* parameters);

/* waits */
void script_wait_for_unit_ready(s32 runtime_id);
void script_opcode_29_wait_walk(s32 unit_id);
void script_opcode_6f_wait_sprite_move(s32 unit_id);
void script_opcode_65_wait_rotate_all(s32 unit_id);
void script_opcode_7e_wait_value(const event_wait_value_parameters_t* parameters);

/* dialog */
void script_opcode_37_flip_message_portrait(const u8* parameters);
void script_opcode_50_portrait_col(s32 portrait_id);

/* camera */
/* A typed operand plus a local Camera cursor reorders the original argument saves. */
void script_focus_speed(u8* parameters, s32* position, s32* rotation);
void script_focus(event_focus_parameters_t* parameters);
void script_apply_relative_camera(event_camera_values_t* parameters, const s32* camera_values);

/* screen and map */
extern u16 g_script_map_darkness_durations[];
/* Script-driven map light transform that map_light_thread interpolates. */
extern SVECTOR g_script_light_angles;
extern VECTOR g_script_light_scale;
extern MATRIX g_script_light_base_direction_matrix;
extern MATRIX g_script_light_base_color_matrix;
extern s16 g_event_map_jump_in_request;
extern s16 g_event_pending_map_jump_out_id;
extern s16 g_event_pending_map_jump_out_2_id;
extern s16 g_event_pending_map_state; /* script variable 0x24 */
extern s16 g_event_weather_request;

void script_opcode_1a_map_darkness(void);
void script_set_specialized_map_destroyed(void);
void script_opcode_3e_color_screen_thread(void);
void script_dark_screen_thread(void);
void script_opcode_41_earthquake_start(void);

/* effects and graphics */
extern s16 g_event_pending_effect_id;
extern s16 g_event_effect_target_mode;
extern s16 g_event_effect_target_x;
extern s16 g_event_effect_target_y;
extern u16 g_3d_object_wait_status;
extern u16 g_field_object_wait_status;

void script_play_effect_and_wait(void);
void script_opcode_8b_effect_end_thread(void);
void script_show_graphic(void);
void script_execute_display_conditions_instruction(void);

/* EVTCHR and VRAM requests */
extern s32 g_script_pending_loaded_evtchr_slot;
extern evtchr_file_record_t* g_script_loaded_evtchr_buffer;
extern s32 g_event_pending_evtchr_save_slot;
extern s32 g_event_pending_loaded_evtchr_clear_slot;
extern s32 g_event_pending_saved_evtchr_clear_slot;
extern s32 g_event_pending_reserved_vram_release_slot;
extern s32 g_event_pending_unit_vram_copy;

/* controller input */
extern u32 g_script_unfiltered_controller_input;
extern u32 g_script_event_input;
extern u32 g_script_event_current_input;
extern u32 g_script_event_previous_input;
extern u32 g_script_event_initial_repeat_counter;
extern u32 g_script_event_repeat_counter;
extern u32 g_script_event_secondary_repeat_counter;
extern u16 g_script_input_suppression_frames;

void script_filter_controller_input(u32* input);
u32* script_get_controller_input_pointer(s32 use_current);
u32 script_get_event_initial_repeat_counter(void);
void script_set_current_otag_entry_and_update_input(u32* entry, s32 input);
void script_update_event_input_state(u32 buttons);
void script_load_event_input_state(const menu_input_state_t* saved);
void script_pulse_tutorial_wait_value(s32 value);

/* debug */
extern const char g_script_pause_message[];
extern s16 g_script_debug_paused;

void script_opcode_16_pause(void);
void script_print_debug_message(void);

/* unreferenced stubs, neutral names */
void script_noop_1(void);
void script_noop_2(void);
void script_noop_3(void);
void script_noop_4(void);
void script_noop_5(void);
void script_noop_6(void);
void script_noop_7(void);
void script_noop_8(void);
void script_noop_9(void);
void script_noop_10(void);
s32 script_return_zero_1(void);

#endif
