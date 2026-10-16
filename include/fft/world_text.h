#ifndef WORLD_TEXT_H
#define WORLD_TEXT_H

/* WORLD's text images, fonts and message boxes. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* text */
extern u8* g_world_text_resident_data[9];
extern text_glyph_state_t g_world_text_message_box_glyph;
extern glyph_blit_t g_world_text_message_box_state;
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

#endif
