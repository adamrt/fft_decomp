#ifndef MENU_TEXT_H
#define MENU_TEXT_H

/* Menu library text: text images, typewriter glyphs, decimal numbers, substitutions and text threads. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* text */
void text_set_blit_destination(s16 destination_x, s16 destination_y);
void menu_render_text_image_at_record_origin(text_dialog_record_t* record);
/* Player-name substitution, padded to 20 bytes with TEXT_END_WAIT_FOR_CONFIRM. */
extern u8 g_text_player_name_buffer[20];
/* Values $00-$1F; indexed substitutions can read past the named 32 entries. */
extern s32 g_text_substitution_values[32];
/* Pixel width of each line from the last text_measure_pixels call. */
extern s32 g_text_line_widths[];
/* Glyph pixel widths, TEXT_GLYPHS_PER_PREFIX entries per prefix page. */
extern const u8 g_text_glyph_widths[];
/* Text IDs select a section in the upper five bits and an entry in the lower eleven. */
extern s32 g_text_section_offsets[TEXT_SECTION_COUNT]; /* the text file's table; the text follows it */
extern u8* g_text_section_pointers[TEXT_SECTION_COUNT];
extern u8* g_text_saved_section_pointers[TEXT_SECTION_COUNT];
extern s32 g_text_section_backup_active;
void text_restore_section_pointers(void);
void text_save_section_pointers(void);
/* shared with the per-program event interpreter and dialog thread */
u8* text_advance_cursor_with_separate_backreference_state(s32* remaining_bytes, u8* cursor, u8** return_cursor);
void menu_display_text(s32 text_id, u8* image, glyph_blit_t* origin, u8* text);
void menu_draw_text_columns_narrow(menu_entry_t* entry, s32* row_offset, u8* buffer);
void text_print_fnt_value_and_next(s32 value);

/* typewriter glyphs */
extern u8* g_text_glyph_bitmap_data;
extern s32 g_text_glyphs_per_wait;
extern s32 g_text_slot_owner_thread_ids[3];
extern s32 g_text_slot_glyph_counts[3];
extern u8 g_text_slot_source_columns[];
extern u8 g_text_slot_row_pitches[];
extern text_glyph_state_t g_text_typewriter_glyph;
extern menu_text_state_t g_text_typewriter_state;
extern RECT g_text_typewriter_vram_rect;
extern u8 g_text_typewriter_glyph_images[3][8][0x54];
extern u8 g_text_typewriter_previous_images[3][0x54];
extern u8 g_text_typewriter_column_scratch[14];
void text_release_typewriter_slot(void);
void text_draw_glyph_with_typewriter_delay(s32 pen_x, s32 pen_y, s32 delay);

/* text threads */
extern s32 g_menu_text_active_key;
void menu_text_start_help_thread(s32 thread_data);
void menu_text_start_thread_if_idle(s32 thread_id, s32 first, s32 second, s32 third);
s32 menu_text_start_or_poll_thread(s32 thread_id, s32 parameter_0, s32 key, s32 parameter_2, s32 y_offset);

/* menu bitstreams */
void menu_bits_init_secondary_reader(const u8* data);

/* encoded text entries */
const u8* menu_text_find_entry_by_index(const u8* data, s16 count, s32 unused_2);

/* decimal text */
extern glyph_blit_t g_menu_decimal_glyph;
extern const char g_text_decimal_format[];
extern glyph_blit_t g_menu_text_pen;
extern s32 g_text_glyph_first_row;
extern s32 g_text_glyph_row_limit;
extern u32 g_text_decimal_divisors[];
extern s32 g_menu_text_param_22; /* line limits copied from the text record (+0x22, +0x24) */
extern s32 g_menu_text_param_24;
void menu_text_render_decimal_entry_list(u8* pixels, const menu_number_entry_t* entries, glyph_blit_t* out, s32 count);
void menu_text_render_signed_decimal_entries(u8* pixels, menu_number_entry_t* entries, glyph_blit_t* out, s32 count);
void text_measure(s16* columns, s16* rows, const u8* text);
void text_measure_pixels(s16* width, s16* rows, const u8* text);
u8* text_find_entry(u32 text_id);
u8* text_advance_cursor_with_backreferences(text_reader_state_t* state, u8* cursor);
/* The menu renderer has a separate linked copy of the same cursor helper. */
u8* menu_text_advance_cursor_with_backreferences(text_reader_state_t* state, u8* cursor);
const u8* text_skip_to_entry(const u8* text, s32 entry_index);
s32 text_count_decimal_digits(s32 value);
s32 text_resolve_format_string_id(s32 format_code, s32 value);
void text_relocate_pointer_table(const u32* offsets);
/* BATTLE and WORLD each link their own body. */
void text_build_unit_name_list(void);
void text_build_display_message(s32 offset, s32 flags, s32* out_width);
void text_select_ability_announcement(struct action_command* action, s32 unit_id, s32 enabled);

/* text substitutions */
/* Text variables $7E/$7F: the four 16-bit values text_set_substitution_values_7e_7f stores
 * (BATTLE 0x801660ec, table index 0x7e from 0x80165ef4; WORLD 0x801533d0 from 0x801531d8). */
extern RECT g_text_substitution_rect;
extern u16 g_text_message_duration_frames;
void text_set_substitution_values_7e_7f(s32 x, s32 y, s32 width, s32 height);
void text_configure_sprite_vram(RECT* rect, s32 width, s32 height, void* packet, s32 clut_offset);
void text_set_message_duration_frames(u16 frames);

/* text data */
extern char g_text_reading_character_message[]; /* "reading charactor%d \n" */
extern s32 g_text_spell_quote_last_ability_id;
extern u8 g_text_spell_quote_exception_skillsets[];
extern u8 g_text_glyph_bitmaps[];

/* glyph rendering */
extern glyph_blit_t g_text_large_number_glyph_rect; /* glyph source rectangle in the large number font */
void menu_clear_render_buffer(void* buffer, u32 bytes);
void text_render_glyph_to_4bpp_image(const u8* glyph_bitmap, u8* image, glyph_blit_t* origin, s32 palette_offset);
void text_blit_glyph(const u8* source, u8* pixels, const glyph_blit_t* glyph, const glyph_blit_t* position);

#endif
