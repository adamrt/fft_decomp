#ifndef WLDCORE_SCRIPT_H
#define WLDCORE_SCRIPT_H

/* WLDCORE's map script and sound novel: opcodes, script state, sound requests and text. */

#include "fft/gfx.h"
#include "fft/wldcore.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* opcode */
extern wldcore_opcode_state_t g_wldcore_opcode_state;
extern void (*g_wldcore_opcode_handlers[])(void);
void wldcore_opcode_apply_pending_delta_and_clamp_value(void);
void wldcore_opcode_map_request_deep_dungeon_entry(void);
void wldcore_opcode_map_request_draw_path(void);
void wldcore_opcode_map_request_erase_path(void);
void wldcore_opcode_map_request_load_event(void);
void wldcore_opcode_map_request_set_var_2(void);
void wldcore_opcode_map_request_text(void);
void wldcore_opcode_start_pending_value_transition(void);
void wldcore_opcode_load_picture(void);
void wldcore_opcode_load_background_set(void);

/* sound */
/* Rows of four sound numbers in the WLDCORE sound bank (id 0xE1), indexed by
 * a sound id from 1. Row 0 is not a sound row: 0x8009ec18 holds the eighth
 * 32-bit value of g_wldcore_tutorial_event_ids (0x1a8). */
extern u8 g_wldcore_sound_quad_command_bytes[][4];
extern s32 g_wldcore_sound_novel_script_sector_offsets[];
extern s32 g_wldcore_sound_novel_text_sector_offsets[];
extern struct suzuki_sound_resource* g_wldcore_sound_novel_sound_resource;
extern s32 g_wldcore_sound_novel_countdown_frames;
extern s32 g_wldcore_sound_novel_countdown_section;
extern s32 g_wldcore_sound_novel_countdown_elapsed;
extern u32 g_wldcore_sound_novel_local_flags[];
extern s32 g_wldcore_sound_novel_wait_counter;
/* Unsigned views of g_wldcore_active_saved_record.text_history_0..3 for the
 * two history shifters, which load them with lhu; the record keeps s16 for
 * the -1 stores of wldcore_reset_saved_record_fields. */
extern s16 g_wldcore_sound_novel_text_history_0;
extern u16 g_wldcore_sound_novel_text_history_1;
extern u16 g_wldcore_sound_novel_text_history_2;
extern u16 g_wldcore_sound_novel_text_history_3;
extern s32 g_wldcore_sound_novel_wait_frames;
extern u8 g_wldcore_sound_novel_quit_help_text_ids[];
void wldcore_sound_process_audio_queue(void);
void wldcore_sound_enqueue_audio_command(wldcore_audio_command_e command, s32 value);
void wldcore_sound_enqueue_music_start(s32 value);
void wldcore_sound_play_effect(s32 sound_id);
void wldcore_sound_wait_for_queue_drain(void);
void wldcore_sound_novel_restore_saved_state(s32 render_index);

/* gfx */
extern s32 g_wldcore_loaded_background_set;
/* Currently resident picture set, reset to -1 by wldcore_init_world_map_state. */
extern s32 g_wldcore_loaded_picture_set;

/* save */
extern s16 g_wldcore_saved_record_index;
/* Saved copy of g_wldcore_context_value_display_position. */
extern point32_t g_wldcore_saved_context_value_display_position;
/* g_main_system_flags as saved by wldcore_gfx_toggle_captured_world_frame (main_system_flags_e). */
extern u32 g_wldcore_saved_system_flags;

/* script */
extern u16* g_wldcore_script_base;
extern void (*g_wldcore_script_opcode_table[])(void);
extern wldcore_script_state_t g_wldcore_script_state;
/* Scalar view of g_wldcore_script_state.flags for the opcode handlers that
 * read-modify-write it: the member spelling there shares one base register
 * with the args/data accesses instead of the target's absolute addresses.
 * The load_* opcode handlers declare volatile views of flags/data/ip locally. */
extern s32 g_wldcore_script_flags;
s32 wldcore_script_process_conditional_set(s32 location, s32 action_mask);
s32 wldcore_script_run_until_yield(void);
u32 wldcore_sound_novel_find_section_instruction(s32 section, s32 opcode);

/* text */
extern s32 g_wldcore_loaded_message_block;
void wldcore_text_get_padded_dimensions(s32 text_id, size32_t* output);
void wldcore_text_init_scrollable_window(wldcore_text_scrollable_window_t* state);
s32 wldcore_text_is_window_finished(wldcore_text_scrollable_window_t* window);

#endif
