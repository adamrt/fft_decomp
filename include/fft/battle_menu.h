#ifndef BATTLE_MENU_H
#define BATTLE_MENU_H

/* Battle menus, window resources and event-overlay menu interfaces. */

#include "fft/battle_ai.h"
#include "fft/menu.h"

/* menu state */
extern s32 g_battle_menu_help_opening;
extern u8 g_battle_menu_restore_depth;
extern u8 g_battle_menu_restore_unit_id;
extern u16 g_battle_menu_transition_state;
extern u16 g_battle_saved_animation_speed; /* animation speed saved while an event thread forces speed 2 */

void battle_menu_clear_status_menu_state(void);
s32 battle_menu_update_menus_and_event_speed(u32* otag, s32 controller_input);

/* input and selection */
extern s16 g_battle_menu_action_slot_selected_option;
extern u16 g_battle_menu_input_block_frames;
extern menu_selection_remap_t g_battle_menu_selection_remaps[8];

/* action menus */
extern s16 g_battle_auto_battle_option_colors[5]; /* text color per option */
extern s16 g_battle_menu_action_slot_row_actions[6];
extern s16 g_battle_menu_action_type_modes[][2];
extern menu_scroll_text_layout_t g_battle_menu_at_list_layout;
/* Row actions of the auto-battle setting menu (menu entry 0x80166b4c, +0x24). */
extern s16 g_battle_menu_auto_battle_row_actions[8];
extern menu_entry_t g_battle_menu_idle_action_entries[];
extern u8 g_battle_menu_idle_action_entry_labels[];
extern u8 g_battle_menu_restriction_message_ids[];
/* The selected command: its kind, then the action command staged for
 * action_preview_at_list. Both scenes alias the action at +4 under its own name. */
extern ai_command_t g_menu_selected_command;
extern action_command_t g_menu_selected_action;
extern s32 g_battle_menu_system_function_command;
extern s32 g_battle_menu_system_function_option;
extern u16 g_battle_menu_system_function_row_actions[];

void battle_menu_build_idle_action_menu(s32 menu_id);
s32 battle_menu_build_unit_action_menus(s32 unit_id, u8* entries, u8* empty, u8* menu_types);
void battle_menu_close_move_help(void);
void battle_menu_dispatch_idle_action_menu(void);
battle_menu_id_e battle_menu_get_id_based_on_mount_moveable_actable(s32 battle_id);
ai_command_t* menu_get_selected_command_address(void);
battle_menu_id_e battle_menu_get_unit_action_menu_id(const unit_stats_t* unit);
s32 battle_menu_init_system_function(s32 type, s32 option, s32 unit_id, s32 parameter, s32 enabled);
void battle_menu_open_active_unit_idle_action_menu(void);
void battle_menu_open_free_cursor_help(void);
void battle_menu_open_free_cursor_mini_menu(void);
void battle_menu_open_help(void);
void battle_menu_open_illegal_move_help(void);
void battle_menu_open_mini_menu(void);
void battle_menu_open_mini_menu_help(void);
void battle_menu_open_move_confirm(void);
void battle_menu_open_non_active_unit_idle_action_menu(void);
void battle_menu_set_next_script_action_menus(void);

/* abilities */
/* Ability-list window layout at 0x80166994. Column modes: 0 text, 1 number,
 * 2 hidden, 3 alternate number. The columns read the work buffer's u16 text
 * ids and values through the layout's s16 pointers. */
extern menu_text_layout_t g_battle_ability_menu_layout;
extern u16 g_battle_menu_ability_display_flags[80];
extern RECT g_battle_menu_ability_list_clut_rect;
extern s16 g_battle_menu_ability_scroll_offset;
extern u8 g_battle_menu_billboard_secondary_skillset_name[];
extern u8 g_battle_menu_billboard_skillset_name[];
extern u8 g_battle_menu_calculator_skillsets[4];
extern s32 g_battle_menu_scroll_list_depth;
extern u8 g_battle_menu_skillset_disabled_flags[]; /* per-row grayed flag */

void battle_menu_build_ability_list(s32 mode);
s32 battle_menu_call_load_charge_skillset(s32 unit_id, u8 skillset_id, s16* ability_ids);
s32 battle_menu_collect_calculator_abilities(s32 unit_id, u8 skillset, s16* out);
s32 battle_menu_collect_draw_out_katanas(s32 unit_id, u8 skillset, u8* out_item_ids);
s32 battle_menu_collect_math_skill_abilities_by_flags(s32 unit_id, u8 skillset, s16* out, u8 mask);
s32 battle_menu_collect_monster_skill_abilities(s32 unit_id, u8 skillset, s16* out, s32 unused, u8* flags_out);
s32 battle_menu_collect_throwable_items(s32 unit_id, u8 ability, u8* items);
s32 battle_menu_display_item_inventory_ability(s32 unit_id, s32 skillset, u8* out_item_ids);
s32 battle_menu_get_unit_skillset_ability_data(
    s32 unit_id, u8 skillset, s16* ability_ids, u8* mp_out, u8* ct_out, s32 unused, u8* flags_out, u8* turns_out);
s32 battle_menu_has_monster_skill_supporter_at_tile(s32 team_flags, s32 x, s32 y, s32 total_height);
s32 battle_menu_init_monster_skill_check(const unit_stats_t* unit);
s32 battle_menu_load_charge_skillset(s32 unit_id, s32 skillset, s16* abilities, u8* ability_ct, u8* turns);
s32 battle_menu_load_draw_out_abilities(s32 unit_id, s32 skillset, u8* out_item_ids);
u16 battle_menu_load_elemental_abilities(s32 skillset_slot, u8 unit_id);
s32 battle_menu_load_item_abilities(s32 unit_id, u8 skillset, u8* out_item_ids);
s32 battle_menu_load_math_skill_attributes(s32 unit_id, s32 skillset, s16* out_ability_ids);
s32 battle_menu_load_math_skill_multiples(s32 unit_id, s32 skillset, s16* out_ability_ids);
s32 battle_menu_load_skillset(s32 unit_id, u8 skillset, u8 menu);
s32 battle_menu_load_throw_abilities(s32 unit_id, s32 skillset, u8* out_item_ids);

/* unit panels */
extern s32 g_battle_formation_display_mode;
extern s32 g_battle_formation_screen_active;
extern menu_number_entry_t g_battle_menu_attack_caster_stats_setup[8];
extern menu_number_entry_t g_battle_menu_attack_target_stats_setup[8];
extern u8 g_battle_menu_billboard_job_name[];
extern u8 g_battle_menu_billboard_unit_name[];
/* Handed to the dead-unit panel thread as its second parameter. Nothing stores to it and the thread
 * does not read the parameter. */
extern s32 g_battle_menu_dead_unit_panel_parameter;
extern u16 g_battle_menu_hovered_stats_frame_count;
extern menu_number_entry_t g_battle_menu_hovered_unit_stats_setup[10];
extern action_data_t* g_battle_menu_preview_target_action;
/* The final two entries of g_battle_menu_attack_caster_stats_setup. */
extern menu_number_entry_t g_battle_menu_projected_value_panel[2];
extern menu_number_entry_t g_battle_menu_right_unit_stats_setup[10];
extern s32 g_battle_post_battle_unit_changes_active;

void battle_menu_build_unit_portrait_poly(POLY_FT4* poly, u32 battle_id);
void battle_menu_copy_unit_data_to_status_billboard(unit_stats_t* unit, menu_unit_status_record_t* output);
s32 battle_menu_display_projected_action_effect(
    POLY_FT4* prim, const point16_t* base_load, const RECT* base_screen, image_load_parameters_t* params);

/* status screen */
extern s32 g_battle_menu_status_enabled;
extern u16 g_battle_menu_status_list_row_actions[8];
extern s32 g_battle_menu_status_requested;
extern s32 g_battle_menu_status_screen_selected;
/* row_offset of the status list's scroll layout, which starts at g_battle_menu_status_visible_rows */
extern s16 g_battle_menu_status_scroll_offset;
extern u16 g_battle_menu_status_scroll_rows;
extern u16 g_battle_menu_status_visible_rows;

void battle_menu_build_unit_status_list(void);
void battle_menu_enter_status_screen_selection(void);
void battle_menu_leave_status_screen_selection(void);

/* numeric displays */

extern menu_quad_page_t g_battle_menu_height_display_pages[2];
extern s8 g_battle_menu_numeric_display_frame_offsets[96];
extern menu_panel_record_t g_battle_menu_numeric_display_panel_records[2];
extern RECT g_battle_menu_numeric_display_texture_window;
extern menu_number_entry_t g_battle_numeric_display_entries[15];
extern RECT g_battle_numeric_display_frame_rect;
extern image_load_parameters_t g_battle_numeric_display_image_params[18];
extern RECT g_battle_numeric_display_text_rect_a;
extern RECT g_battle_numeric_display_text_rect_b;
extern RECT g_battle_numeric_display_text_rect_c;
extern RECT g_battle_numeric_display_texture_window;

/* windows and sprites */
/* BATTLE scratch at 0x801cc074, in the event-overlay area past BATTLE.BIN:
 * battle_menu_build_sprite_page lays it out as two sprite pages and the
 * scrolling ability list saves its five 16-color CLUTs there. WORLD keeps
 * its own g_world_menu_panel_buffer at the same address. */
typedef union battle_menu_scratch_buffer {
    menu_sprite_page_t sprite_pages[2];
    u16 clut_save[5 * 16];
} battle_menu_scratch_buffer_t;

/* Live caster-panel image parameters followed by their defaults. */
extern image_load_parameters_t g_battle_menu_attack_caster_quad_image_params[2][5];
extern image_load_parameters_t g_battle_menu_icon_strip_image_params[20];
extern battle_menu_scratch_buffer_t g_battle_menu_scratch_buffer;
extern image_load_parameters_t g_battle_menu_single_icon_image_params[5];
/* Entries 5..7 above; a separate alias preserves the original template base. */
extern image_load_parameters_t g_battle_menu_sprite_page_image_param_bases[];
extern image_load_parameters_t g_battle_menu_sprite_page_image_params[30];
extern RECT g_battle_menu_sprite_page_texture_windows[5];
extern menu_unit_editor_frame_t g_battle_menu_unit_editor_frames[2];
extern glyph_blit_t g_battle_menu_window_sprite_glyph;
extern glyph_blit_t g_battle_menu_window_sprite_position;

u8* battle_menu_build_and_upload_window_frame_image(s32 width, s32 height, RECT* rect, s32 mode);
void battle_menu_build_and_upload_window_image(s32 width, s32 height, RECT* rect, s32 mode, s32 tail_offset);
void battle_menu_build_sprite_page(s32 page_index, RECT* base_screen);
void battle_menu_build_window_sprites(RECT* rect, menu_entry_t* param, menu_icon_record_t* record);
void battle_menu_init_semitransparent_sprt(SPRT* sprite);

/* text rendering */
extern u16* const* g_battle_help_active_text_id_tables;
/* Source image used when battle overlays render menu glyphs. */
extern menu_glyph_sheet_t g_battle_menu_glyph_image_data;
extern s16 g_battle_menu_status_left_text_ids[20];
extern s16 g_battle_menu_status_right_text_ids[20];
extern s32 g_battle_menu_text_param_22; /* 0x80166020: record +0x22 */
extern s32 g_battle_menu_text_param_24; /* 0x80166024: record +0x24 */
extern glyph_blit_t g_battle_menu_text_pen;
extern s32 g_battle_menu_text_section_offsets[];

void battle_clear_menu_render_buffer(void* buffer, u32 bytes);
void battle_menu_display_text(u32 text_id, u8* image, glyph_blit_t* origin, u8* text);
void battle_menu_draw_text_columns_narrow(menu_entry_t* entry, s32* row_offset, u8* buffer);
u8* battle_menu_redraw_text_page_on_scroll_in_rect(
    menu_entry_t* entry, s32* row_offset, s32* redraw, s32 unused, RECT* area);

/* palettes and fades */
extern menu_palette_pair_t g_battle_menu_default_palette_colors;
extern s32 g_battle_menu_panel_fade_intensity;
extern s32 g_battle_menu_panel_fade_mode; /* menu_panel_fade_mode_e */
extern u8 g_battle_menu_panel_fade_modes[];
extern menu_palette_slot_t g_battle_numeric_display_palettes[];

/* buffer allocation */
enum { BATTLE_MENU_BUFFER_PAGE_COUNT = 53 };

extern menu_allocation_t g_battle_menu_allocator_slot_records[MENU_BUFFER_ALLOCATION_COUNT];
extern u8 g_battle_menu_buffer_allocation_map[BATTLE_MENU_BUFFER_PAGE_COUNT];
extern u8 g_battle_menu_buffer_arena[BATTLE_MENU_BUFFER_PAGE_COUNT][MENU_BUFFER_PAGE_SIZE];

void battle_menu_init_buffer_allocator(void);
void battle_menu_release_current_thread_slot(void);

/* overlay loading */

/* threads */

s32 battle_menu_is_still_building(void);

#endif
