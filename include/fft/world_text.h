#ifndef WORLD_TEXT_H
#define WORLD_TEXT_H

/* WORLD's text: the resident and bound text sections, the message box, and text rendering and measuring. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_list.h"
#include "fft/text.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* text sections */
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

enum { WORLD_TEXT_ID_FORMATION_TEXT_BASE = WORLD_TEXT_SECTION_FORMATION_TEXT << TEXT_ID_SECTION_SHIFT };

extern s32 g_world_text_section_offsets[];
extern u8 g_world_text_shop_menu_labels_data[];
extern u8 g_world_text_name_entry_keys_data[];
u8* world_text_get_resident_data(world_text_data_e index);
u8* world_text_bind_section_data(u32 index);
/* MAIN selects the WORLD address of text_find_entry through this existing alias. */
u8* world_text_find_entry(u32 text_id);
u8* world_text_advance_entry_by_char_count(u8* text, s32 count);
s16 world_text_get_encoded_char_at_position(u8* table, s16 index, s32 position);

/* message box */
extern glyph_blit_t g_world_text_message_box_state;
extern text_glyph_state_t g_world_text_message_box_glyph;
extern s32 g_world_text_message_box_glyph_delay;
extern s32 g_world_text_message_box_digit_divisors[];
extern s32 g_world_text_message_box_slot_threads[3];
extern s32 g_world_text_message_box_slot_glyph_counters[3];
extern u8 g_world_text_message_box_slot_carry_offset[4];
extern u8 g_world_text_message_box_slot_prev_width[4];
extern RECT g_world_text_message_box_vram_rect;
extern u8 g_world_text_message_box_glyph_images[3][8][84];
extern u8 g_world_text_message_box_previous_images[3][84];
extern u8 g_world_text_message_box_column_scratch[14];
extern u8 g_world_text_task_phase;
extern SVECTOR g_world_text_partner_screen_coords;
void world_text_message_box_thread(void);
s32 world_text_is_printing(s32 thread_id);
void world_text_resume_printing(s32 thread_id);
void world_text_show_message_and_play_sound(s32 message_id, s32 sound_id);

/* text images */
extern menu_list_page_t g_world_text_list_pages[2];
void world_text_render_id_list_to_image_rows(u8* font, s16* text_ids, RECT* destination, s32 palette);
void world_text_kern_into_command_buffer(u8* text, s16* name, RECT* rect, s32 palette);
s32 world_text_measure_entry_pixel_width(const u8* entry);
void world_text_build_roster_name_list(void);

#endif
