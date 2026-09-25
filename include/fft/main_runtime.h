#ifndef FFT_MAIN_RUNTIME_H
#define FFT_MAIN_RUNTIME_H

#include "fft/battle_gfx.h"
#include "fft/data.h"
#include "fft/main_file.h"
#include "fft/main_zodiac.h"
#include "fft/options.h"
#include "fft/zodiac.h"
#include "psx/etc.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Forward declarations for types defined in other module headers. */
struct battle_stats;
struct battle_deployed_coords;

extern u8 g_main_ability_aoe_data[];
extern u8 g_ability_ct_data[];
extern s32 g_active_graphics_buffer_index;
extern s32 g_animation_speed;
extern battle_render_buffer_t g_battle_buffer_a[2];
extern s32 g_battle_frame_counter;
extern s32 g_battle_frame_measurement;
extern int g_battle_overlay_loaded;
extern s32 g_controller_input_pressed;
extern s32 g_controller_input_previous;
extern s32 g_controller_input_raw;
extern u32 g_controller_input_released;
extern u32 g_controller_pressed_copy_0;
extern u32 g_controller_pressed_copy_1;
extern u32 g_controller_pressed_copy_2;
extern u32 g_controller_pressed_copy_3;
extern u32 g_controller_previous_copy_0;
extern u32 g_controller_previous_copy_1;
extern u32 g_controller_previous_copy_2;
extern u32 g_controller_previous_copy_3;
extern u32 g_controller_stable_frames;
extern entd_unit_t* g_current_entd_unit;
extern volatile u8 g_fntload_clut_y_offset_immediate;
extern s32 g_font_print_enabled;
extern s32 g_frame_pacing;
extern s32 g_frame_pacing_suppressed;
extern s32 g_frame_pacing_timer;
extern u8 g_geomancy_terrain_ability_table[];
extern u8 g_highest_party_level;
extern u8 g_main_item_quantities[ITEM_ID_COUNT];
extern job_data_t g_job_data[];
extern job_data_t* g_job_data_pointer;
extern const u16 g_job_level_jp_requirements[8];
extern ability_secondary_data_t g_main_ability_range_data[];
extern u8* g_main_brave_story_ages_source;
/* Memory-card BIOS event handles opened by main_card_init_events; the CARD,
 * OPEN and WORLD pollers test them by these indices. */
enum {
    MAIN_CARD_EVENT_BIOS_DONE,
    MAIN_CARD_EVENT_BIOS_ERROR,
    MAIN_CARD_EVENT_BIOS_TIMEOUT,
    MAIN_CARD_EVENT_BIOS_NEW_CARD,
    MAIN_CARD_EVENT_HARDWARE_DONE,
    MAIN_CARD_EVENT_HARDWARE_ERROR,
    MAIN_CARD_EVENT_HARDWARE_TIMEOUT,
    MAIN_CARD_EVENT_HARDWARE_NEW_CARD,
    MAIN_CARD_EVENT_COUNT
};
extern s32 g_main_card_bios_events[MAIN_CARD_EVENT_COUNT];
extern u8 g_main_card_bios_events_opened;
extern RECT g_main_gfx_frame_image_rect;
extern RECT g_main_gfx_frame_palette_rect;
extern RECT g_main_gfx_frame_palette_tail_rect;
extern u32 g_main_gfx_now_loading_clut[];
extern u32 g_main_gfx_now_loading_frame_counter;
extern u32 g_main_gfx_now_loading_otags[2][2];
/* Seven textured quads per screen polarity; main_gfx_add_now_loading_to_otag
 * walks the same table as a [7][sizeof(POLY_FT4)] byte array. */
extern POLY_FT4 g_main_gfx_now_loading_primitives[2][7];
extern u32 g_main_gfx_now_loading_visible;
extern main_zodiac_screen_position_t g_main_gfx_zodiac_screen_position;
extern u8 g_main_gfx_zodiac_sprite_count;
extern main_zodiac_sprite_frame_t g_main_gfx_zodiac_sprite_frames[];
extern main_zodiac_scale_t g_main_gfx_zodiac_sprite_scales[];
extern main_zodiac_texture_position_t g_main_gfx_zodiac_texture_position;
/* 64 allocation tags, one per 0x800-byte block in the game arena. */
extern u8 g_main_heap_game_allocator_table[64];
extern u8 g_main_heap_smd_allocator_table[16];
extern u8 g_main_heap_smd_base[];
extern u32 g_main_input_raw_buttons;
extern u8 g_main_input_repeat_initial_delay;
extern u8 g_main_input_repeat_period;
extern u8 g_main_input_secondary_repeat_period;
extern u8 g_main_menu_scroll_accel_delay;
extern u8 g_main_menu_scroll_slow_step;
extern u8 g_main_menu_scroll_fast_step;
extern u8 D_800473A7;
extern u8 g_main_accessory_page_order[0x21];
extern u8 g_main_armor_page_order[0x25];
extern item_attribute_t g_main_item_attributes[];
extern u8 g_main_helmet_page_order[0x1d];
extern u8 g_main_item_page_order[0x15];
/* The first 0x40 bytes are Move-Find-Item flags; the second half is not yet
 * identified. Both halves are initialized and serialized as one block. */
extern u8 g_main_item_location_flags[0x80];
extern u8 g_main_item_poached_quantities[ITEM_ID_COUNT];
extern item_secondary_data_t g_main_item_secondary_data[];
/* &g_main_item_primary_data[0].secondary_data_id viewed as a stride-12 byte
 * array. main_unit_set_equipment_stats reads the secondary id through its own
 * %lo(g_main_item_primary_data+4) address while the type_flags test uses a
 * record pointer; reading both through item_data_t folds them onto one base
 * register and misses the byte-exact match. */
extern u8 g_main_item_secondary_data_ids[];
/* 0x80057b20..0x80057b5b: seven independent signed-byte order lists.
 * main_item_init_order_tables establishes capacities 12/8/8/8/8/8/8 with -1
 * terminators; member categories remain unproven. */
typedef struct item_type_order_tables {
    s8 order_0[12];
    s8 order_1[8];
    s8 order_2[8];
    s8 order_3[8];
    s8 order_4[8];
    s8 order_5[8];
    s8 order_6[8];
} item_type_order_tables_t;

extern item_type_order_tables_t g_main_item_type_order_tables;
/* Views of g_main_item_type_order_tables.order_1..6 for the WORLD save-image
 * copies (world_card_build_save_image, world_card_load_globals_from_save_image),
 * whose code the member spelling changes. */
extern s8 g_main_item_type_order_1[8];
extern u8 g_main_item_type_order_2[7];
extern u8 g_main_item_type_order_3[5];
extern u8 g_main_item_type_order_4[5];
extern u8 g_main_item_type_order_5[7];
extern u8 g_main_item_type_order_6[5];
extern weapon_data_t g_main_item_weapon_data[]; /* 0x80063ab8 */
extern u8 g_main_weapon_page_order[0x8a];
extern u16 g_main_job_jp_requirements_by_level[];
extern u8 g_main_action_menu_types_by_skillset[0xe0];
extern game_options_t g_main_game_options;
extern volatile game_options_t g_main_game_options_defaults;
extern u8 g_main_save_proposition_count;
extern u8 g_main_sound_env_sed_data[];
extern u8 g_main_sound_system_sed_data[];
extern s32 g_main_system_frontend_world_result;
extern u32 g_main_system_game_flow_state;
extern u32 g_main_system_game_loop_stack_pointer;
extern u8 g_main_system_go_straight_to_battle;
extern int g_main_system_graphics_error;
extern s32 g_main_system_play_time_frames;
extern s32 g_main_system_play_time_hours;
extern s32 g_main_system_play_time_minutes;
extern s32 g_main_system_play_time_seconds;
extern u32 g_main_system_flags;
extern s32 g_max_battle_frame_measurement;
extern main_file_load_descriptor_t g_open_file_header;
extern u8 g_open_file_location[];
extern u8 g_party_slot_base;
extern POLY_GT4 g_require_gfx_poly_gt4_array_32[];
extern POLY_GT4 g_require_gfx_poly_gt4_array_8[];
extern POLY_GT4 g_require_gfx_poly_gt4_banks[2][40];
extern u16 g_scenario_event_finish_operations[];
extern volatile u32* g_spu_delay_reg_pointer;
extern volatile u32* volatile g_spu_dpcr_pointer;
extern s32 g_st_cd_interrupt_pending;
extern const u16 g_zodiac_day_limits[ZODIAC_SIGN_ORDINARY_COUNT];
extern u8 table[0x100];

/* Unnamed data, in address order. */
extern s32 g_main_battle_bin_sector;
extern u8 g_main_common_menu_graphics_loaded;
extern s32 g_unit_graphics_load_count;
extern u32 g_main_tutorial_progress_masks[];
extern u8 g_main_special_portrait_unit_id;
extern s32 g_main_saved_animation_speed;
extern s32 g_main_saved_weather_variable;
extern u8 D_80057b1c;
/* Word-indexed RAM bitmaps; the save serializer copies only their packed data
 * bytes and omits the padding at the end of each allocation. */
extern u32 g_main_proposition_last_attempt_date_bits[27];
extern u32 g_main_treasure_acquisition_date_bits[14];
extern u32 g_main_land_discovery_date_bits[5];
extern u8 g_main_jump_charge_ability_data_by_ability_id[];
extern u8 g_main_item_ability_item_ids_by_ability_id[];
extern u8 g_main_throw_ability_item_types_by_ability_id[];
extern u8 g_main_item_ability_item_ids[14];
extern u8 g_main_calculator_primes[CALCULATOR_PRIME_COUNT];
extern u16 g_main_month_start_day_offsets[13];
extern struct battle_deployed_coords* g_main_current_formation_entry;
extern u8 g_main_crystal_learnable_abilities[];
extern u8 g_main_crystal_treasure_item_id;
extern u8 g_main_brave_story_character_ages[0x40];
extern s32 g_main_debug_display_enabled;
extern s32 g_main_entd_current_event_id;
extern entd_file_t* g_main_entd_set;
extern u8 g_main_proposition_states[96];
extern s32 g_main_replay_story_event_index;
extern u32 g_main_save_day;
extern u32 g_main_save_month;
extern u8 g_main_save_slot_buffer[];
extern u32 g_main_saved_data_bits[40];
extern s32 g_main_script_variables[0x100];
extern s32 g_main_system_session_frames;

void init_card_earlysafe(s32 val);

/* file */
/* File-header wrapper at 0x80011bd0 forwards build's result. */
s32 main_file_build_header_nnl(main_file_load_descriptor_t* header, s32 sector, s32 sectors, void* destination);
s32 main_file_call_build_header(s32 sector, s32 size, void* destination);
int main_file_is_still_loading(void);
void main_file_reset_cd_subsystems(void);

/* func */
void main_noop_800449ec(void);

/* item */
void main_item_init_new_game_inventory(void);

/* noop */
/* 0x800449f8: a return-only hook. Main and overlay callers supply two words,
 * but neither their original meaning nor the hook's original name is proven. */
void main_noop_800449f8(s32 category, s32 code);

/* overlay */
void main_overlay_call_battle_entrypoint(void);
void main_overlay_exec_battle_bin(void);
int main_overlay_exec_open_bin_main_loop(int mode);
void main_overlay_exec_open_bin_ending(void);
int main_overlay_open_world_and_wldcore(int load_world);
void main_overlay_open_world_bin(s32 mode);

/* restore */
void main_restore_game_loop_stack_pointer(u32* source);

/* sound */
void main_sound_set_sfx_echo(int sound_id, int echo);
void main_sound_stop_weather_sfx_music(void);

/* system */
void main_system_handle_animation_exception(int error_code);
void main_system_handle_battle_load_exception(s32 value);
void main_system_handle_malloc_exception(s32 allocator_id, s32 error_code);
void main_system_handle_pointer_exception(int error_code);
void main_system_reset_game(void);
void main_system_run_game_loop(void);
void main_system_stop_display_and_audio_for_game_reset(void);
void main_system_store_stack_pointer(u32* destination);

/* unit */
void main_unit_update_formation_stats(struct battle_stats* unit);

/* util */
s32 main_util_roll_pass_fail(s32 maximum, s32 threshold);

s32 CdStatus(void);

extern u8 g_main_special_portrait_wldface_id;

extern s32 D_800459D8;
extern u8 D_8005E950[];
/* Pre-biased so the raw ability id indexes the SCUS secondary-data byte; see
 * main_ability_calculate_pointers_and_type. */
extern u8 g_main_math_rsm_ability_data_by_ability_id[];
extern u8* g_event_overlay_load_address;
extern s32 g_main_deployed_unit_map_coordinates[];
extern u8 g_main_terrain_movement_cost_tables[][64];
extern const u8 g_main_terrain_status_flags[64];
/* Provisional: 12 item types indexed by throw ability id - 0x17e. */
extern u8 g_main_throw_ability_item_types[];
u8* get_item_data_pointer(void);
s32 get_total_equipment_quantity(s32 item_id, s32 include_equipped);

#endif
