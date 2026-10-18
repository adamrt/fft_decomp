#ifndef SCRIPT_H
#define SCRIPT_H

/* The event script interpreter for BATTLE and WORLD: its state, the event loader and scenario tables, the instruction
 * handlers and the requests they leave for the frame loop, and the request state BATTLE and WORLD keep apart. The
 * bytecode is in script_instruction.h and the script variables in script_variable.h. */

#include "fft/battle_map.h"
#include "fft/command.h"
#include "fft/gfx.h"
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
/* The finish operation of a scenario's finish word: its high bits, as script_get_event_finish_operation
 * returns them. Another nonzero value acts as TO_WORLD and also clears the current event and ENTD. */
enum { SCRIPT_FINISH_TO_WORLD = 0x80, SCRIPT_FINISH_NEXT_EVENT = 0x81, SCRIPT_FINISH_RESET = 0x82 };

extern event_file_block_t g_script_event_block_buffer;

/* The EVENT/TEST.EVT block of the loaded event. */
extern event_file_block_t* g_script_event_block;

/* Operand-byte counts per opcode, excluding the opcode byte; not per-operand
 * width schemas. The table ends after opcode 0xf3. */
extern u8 g_script_event_instruction_sizes[0xf4];

void script_load_event(s32 id);
s32 script_load_next_event(void);
void script_reset_event_state(void);
s32 scenario_get_finish_operation(void);
u32 script_get_event_finish_operation(void);

/* scenario */
/* ATTACK reuses the setup records and condition blocks for deployment text after loading them. */
extern u8 g_scenario_table[16384];
extern u8 g_scenario_event_condition_blocks[];

/* Resident copies survive that reuse. The condition runner inspects only the first ten offsets. */
extern u16 g_scenario_event_finish_operations[512];
extern u16 g_scenario_condition_offsets[16];
extern s16 g_scenario_condition_data[256];
extern s32 g_scenario_condition_unit_battle_id;

s32 scenario_load_conditionals(void);
s32 scenario_run_conditions(void);
void scenario_restore_music_and_darkness(void);

/* interpreter */
/* script_run_event_frame results: RUNNING (or g_script_frame_result_override) while the script
 * threads run; once they have all stopped, NO_FINISH or what the scenario's finish operation asks
 * for. Tutorial input mode reports NO_FINISH every frame. */
typedef enum script_event_frame_result {
    SCRIPT_EVENT_FRAME_NO_FINISH = 0, /* the battle goes on */
    SCRIPT_EVENT_FRAME_RUNNING = 1,
    SCRIPT_EVENT_FRAME_TO_WORLD = 9,
    SCRIPT_EVENT_FRAME_NEXT_EVENT = 10,
    SCRIPT_EVENT_FRAME_RESET = 11,
    SCRIPT_EVENT_FRAME_ENDING = 19 /* the Funeral event (EVENT_ID_FUNERAL) has ended */
} script_event_frame_result_e;

extern s32 g_script_event_speed;
extern u16 g_script_frame_result_override;
extern s16 g_scenario_event_active;
extern s16 g_script_threads_idle;

void script_run_next_event(void);
void script_run_next_event_unused(void);
void script_start_current_event_thread(void);
s32 script_run_event_frame(u32* ot, u32 buttons);
void script_frame_after_requests_stub(void);
void script_frame_after_threads_stub(void);
void script_set_event_speed(s32 speed);

/* BATTLE and WORLD each have their own body. */
void script_execute_event(void);

/* opcode handlers */
void script_opcode_16_pause(void);
void script_opcode_1a_map_darkness(void);
void script_opcode_28_walk_to(const event_walk_to_parameters_t* parameters);
void script_opcode_29_wait_walk(s32 unit_id);
void script_opcode_2a_block_start_thread(void);
void script_opcode_32_color_unit(const event_color_unit_parameters_t* parameters);
void script_opcode_37_flip_message_portrait(const u8* parameters);
void script_opcode_3e_color_screen_thread(void);
void script_opcode_41_earthquake_start(void);
void script_opcode_45_add_unit(void);
void script_opcode_50_portrait_col(s32 portrait_id);
void script_opcode_65_wait_rotate_all(s32 unit_id);
void script_opcode_68_mirror_sprite(const event_mirror_sprite_parameters_t* parameters);
void script_opcode_69_face_tile(const event_face_tile_parameters_t* parameters);
void script_opcode_6f_wait_sprite_move(s32 unit_id);
void script_opcode_70_jump(const event_jump_parameters_t* parameters);
void script_opcode_74_unknown(const u8* unused_parameters);
void script_opcode_7a_dismiss_unit(s32 sprite_set);
void script_opcode_7e_wait_value(const event_wait_value_parameters_t* parameters);
void script_opcode_80_march(const event_march_parameters_t* parameters);
void script_opcode_81_set_unit_movement_effect_suppression(const event_movement_effect_parameters_t* parameters);
void script_opcode_82_load_attack_graphics(void);
void script_opcode_83_change_stats(const event_change_stats_parameters_t* parameters);
void script_opcode_85_unlock_date(u32* bitset, s32 date_index, s32 month, s32 day);
void script_opcode_8b_effect_end_thread(void);
void script_opcode_8c_unit_anim_rotate(const event_unit_anim_rotate_parameters_t* parameters);
void script_opcode_94_teleport_out(s32 unit_id, s32 remove);
void script_opcode_97_reset_palette(const event_unit_parameters_t* parameters);
void script_opcode_98_teleport_in(s32 unit_id, s32 unused);
void script_opcode_99_blue_remove_unit(s32 unit_id);

/* AddGhostUnit: when runtime_id is free, queue the graphics of a sprite with no unit stats at the
 * given tile and facing and return 1; return 0 when the id is taken. */
s32 script_opcode_47_add_ghost_unit(
    s32 map_x, s32 map_y, s32 map_level, s16 facing, s16 spritesheet_id, s32 runtime_id, s32 flags);

/* Empty in WORLD, which links its own stub under this name. */
void script_opcode_49_add_unit_start_thread(void);

/* instruction scanning */
s32 script_find_instruction_byte_offset(s32 offset, s32 opcode);

/* Returns the offset after the matching target marker; a missing target stops
 * the current thread. alternate_opcode accepts -1. */
s32 script_find_jump_target(s32 limit_offset, event_opcode_e target_opcode, s32 alternate_opcode, s32 target_id);

/* Reads an unaligned little-endian signed 16-bit operand. */
s16 script_read_s16(const void* source);

/* values */
void script_seed_random_from_vsync(void);
void script_clamp_s32_to_range(s32* value, s32 minimum, s32 maximum);
s32 script_scale_music_volume(s32 volume);

/* units */
/* Four unit bindings used by the scenario's slot selectors. */
extern s16 g_script_unit_slots[EVENT_UNIT_SELECTOR_SLOT_COUNT];

void script_blue_remove_all_enemy_units(void);
void script_inflict_status_thread(void);
void script_process_inflict_status_commands(void);

/* Event-unit lookup uses the resident scene's interpreter state. */
s32 script_resolve_runtime_id(s32 unit_id);

/* unit movement and animation */
/* Task words of a sprite-move thread. The SpriteMove instruction and the
 * thread itself store the moving unit's runtime id; WaitSpriteMove matches it. */
enum { SPRITE_MOVE_THREAD_WORD_RUNTIME_ID = 0 };

void script_opcode_5f_warp_unit(const event_warp_unit_parameters_t* instruction);
void script_warp_unit_display_to_paired_unit(s32 runtime_id);
void script_sprite_move(void);
void script_sprite_move_beta(void);
void script_opcode_11_unit_anim(const event_unit_anim_parameters_t* parameters);
void script_opcode_2c_face_unit(const event_face_unit_parameters_t* parameters, s32 second_only);
void script_opcode_2d_rotate_unit(const event_rotate_unit_parameters_t* parameters);
void script_set_units_palette_update_suppression(s32 event_unit_id, s32 enable);
void script_wait_for_unit_ready(s32 runtime_id);

/* camera */
void script_focus(event_focus_parameters_t* parameters);
void script_apply_relative_camera(event_camera_values_t* parameters, const s32* camera_values);

/* A typed operand plus a local Camera cursor reorders the original argument saves. */
void script_focus_speed(u8* parameters, s32* position, s32* rotation);

/* screen and map */
extern s16 g_event_map_jump_in_request;
extern s16 g_event_pending_map_jump_out_id;
extern s16 g_event_pending_map_jump_out_2_id;
extern s16 g_event_pending_map_state; /* script variable 0x24 */
extern s16 g_event_weather_request;

/* The gradient colors of the Background instruction. */
extern battle_map_background_gradient_colors_t g_event_background_colors;

/* Script-driven map light transform that battle_map_light_thread interpolates. */
extern SVECTOR g_script_light_angles;
extern VECTOR g_script_light_scale;
extern MATRIX g_script_light_base_direction_matrix;
extern MATRIX g_script_light_base_color_matrix;

void script_set_specialized_map_destroyed(void);
void script_dark_screen_thread(void);

/* effects and graphics */
/* BATTLE and WORLD each define these request cells. The Effect instruction stores the tile coordinates,
 * but the request handoff uses only the runtime ID and mode. */
extern s16 g_event_pending_effect_id;
extern u16 g_event_effect_target_runtime_id;
extern s16 g_event_effect_target_mode;
extern point16_t g_event_effect_target_position;
extern u16 g_3d_object_wait_status;
extern u16 g_field_object_wait_status;

void script_play_effect_and_wait(void);
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

/* attack graphics preload */
/* Whether an event instruction loaded the ATTACK overlay and its graphics
 * before the deployment screen starts. */
typedef enum script_attack_entry_mode {
    SCRIPT_ATTACK_ENTRY_NORMAL = 1,      /* set when an event thread starts */
    SCRIPT_ATTACK_ENTRY_PRELOADED = 2,   /* script_opcode_82_load_attack_graphics ran */
    SCRIPT_ATTACK_ENTRY_PRELOAD_USED = 3 /* deploy_open skipped its loads */
} script_attack_entry_mode_e;

extern u16 g_script_attack_entry_mode; /* script_attack_entry_mode_e */

/* controller input */
/* The values tutorial_pulse_wait_value holds in EVENT_SCRIPT_VAR_TUTORIAL_WAIT_VALUE as a menu
 * screen opens, so that a tutorial script can wait for it. A system function pulses its own id
 * instead. */
typedef enum script_tutorial_wait_value {
    SCRIPT_TUTORIAL_WAIT_AT_LIST = 0xf9,
    SCRIPT_TUTORIAL_WAIT_ABILITY_AT_LIST = 0xfa, /* the AT list preview of the highlighted ability */
    SCRIPT_TUTORIAL_WAIT_ABILITY_LIST = 0xfb,
    SCRIPT_TUTORIAL_WAIT_SKILLSET_LIST = 0xfc,
    SCRIPT_TUTORIAL_WAIT_GAME_OPTIONS = 0xfd,
    SCRIPT_TUTORIAL_WAIT_MINI_MENU = 0xfe, /* the Triangle menu */
    SCRIPT_TUTORIAL_WAIT_MENU_ENTRY = 0xff /* menu_start_entry_thread opened a menu */
} script_tutorial_wait_value_e;

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

/* Called by each program's native thread scheduler to route event input. */
void script_route_event_input(void);
void script_update_event_input_state(u32 buttons);
void script_load_event_input_state(const menu_input_state_t* saved);

/* debug */
extern s16 g_script_opcode_dc_value; /* EVENT_OPCODE_UNKNOWN_DC stores 13; nothing reads it */

void script_print_debug_message(void);

/* unreferenced stubs */
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

/* event requests */
/* BATTLE and WORLD each define these input and request records. WORLD stores the
 * map/object requests but has no pending-request consumer. */
extern u32 g_event_saved_input;      /* event input held while a script runs */
extern u16 g_event_map_reload_state; /* 1 starts, 2 polls, >=3 is reset by the outer update */
extern u16 g_event_map_append_state;
extern u16 g_event_3d_object_use_request;
extern u16 g_event_field_object_use_request;

/* battle event state */
extern s32 g_event_frame_counter; /* counts the frames the event interpreter runs */
extern u16 g_event_scenario_music_1_id;
extern u16 g_event_scenario_music_2_id;

/* EVENT_OPCODE_USE_3D_OBJECT and EVENT_OPCODE_USE_FIELD_OBJECT set a request with their two
 * operands; script_process_pending_requests passes them to battle_map_set_3d_object_state
 * and battle_map_texture_start_animation, and the matching wait instructions poll until
 * the object is done. Provisional: the operands' value domains. The consumer reads the u16
 * requests and operands with lhu; every other access is a store. */
extern u16 g_event_3d_object_id;
extern u16 g_event_3d_object_state;
extern u16 g_event_map_command_83_arg1; /* the field object's texture animation id */
extern u16 g_event_map_command_83_arg2;

/* The volume and fade of a music switch; the switch itself and the other music requests are in
 * sound.h. */
extern u16 g_event_music_switch_fade;   /* the consumer multiplies it by 4 */
extern u16 g_event_music_switch_volume; /* scaled 0..96 to 0..127 */

/* world event state */
/* Operand pairs that one WORLD script_execute_event opcode stores together;
 * each second 16-bit value sits off a 32-bit alignment boundary in WORLD bss, so each pair is
 * one object. */
typedef struct script_command_args {
    s16 arg1;
    s16 arg2;
} script_command_args_t;

typedef struct script_music_switch {
    s16 volume;
    s16 fade;
} script_music_switch_t;

#endif
