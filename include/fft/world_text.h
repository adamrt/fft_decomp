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
/* Resident text resource indices, used by screen_text_get_resident and
 * screen_text_bind_section. */
typedef enum screen_text_data {
    SCREEN_TEXT_DATA_SKILLSET_NAMES = 0,
    SCREEN_TEXT_DATA_JOB_NAMES = 1,
    SCREEN_TEXT_DATA_ABILITY_NAMES = 2,
    SCREEN_TEXT_DATA_FORMATION_MESSAGES = 3,   /* TEXT_SECTION_FORMATION_MESSAGES */
    SCREEN_TEXT_DATA_DISMISS_UNIT_REPLIES = 4, /* SCREEN_TEXT_SECTION_FORMATION_TEXT */
    SCREEN_TEXT_DATA_JOB_DESCRIPTIONS = 5,     /* SCREEN_TEXT_SECTION_FORMATION_TEXT */
    SCREEN_TEXT_DATA_NAME_ENTRY_KEYBOARD = 7,
    SCREEN_TEXT_DATA_SHOP_DIALOGUE = 8, /* SCREEN_TEXT_SECTION_SHOP_DIALOGUE */
} screen_text_data_e;

/* WORLD text files materialize 32 section pointers after their 0x80-byte
 * offset/header area. The same table is saved and restored around overlays. */
enum {
    SCREEN_TEXT_SECTION_TABLE_BYTES = 128,
    SCREEN_TEXT_FILE_HEADER_BYTES = 128,
};

/* The text sections WORLD's screens bind while they are up (text_section_e
 * has the resident ones). */
typedef enum screen_text_section {
    SCREEN_TEXT_SECTION_TUTORIAL_MESSAGES = 23, /* the running tutorial's message file */
    SCREEN_TEXT_SECTION_NAME_ENTRY_KEYS = 28,   /* name entry: the key rows */
    SCREEN_TEXT_SECTION_FORMATION_TEXT = 29,    /* formation: what a dismissed unit says, or the job wheel's
                                                 * descriptions, whichever was bound last */
    SCREEN_TEXT_SECTION_SHOP_MENU_LABELS = 30,
    SCREEN_TEXT_SECTION_SHOP_DIALOGUE = 31, /* the shopkeeper's lines */
} screen_text_section_e;

enum { TEXT_ID_FORMATION_TEXT_BASE = SCREEN_TEXT_SECTION_FORMATION_TEXT << TEXT_ID_SECTION_SHIFT };

extern s32 g_screen_text_section_offsets[];
extern u8 g_screen_text_shop_menu_labels_data[];
extern u8 g_screen_text_name_entry_keys_data[];
u8* screen_text_get_resident(screen_text_data_e index);
u8* screen_text_bind_section(u32 index);
/* MAIN selects the WORLD address of text_find_entry through this existing alias. */
u8* world_text_find_entry(u32 text_id);
u8* screen_text_advance_chars(u8* text, s32 count);
s16 screen_text_get_char_at(u8* table, s16 index, s32 position);

/* message box */
extern glyph_blit_t g_message_box_state;
extern text_glyph_state_t g_message_box_glyph;
extern s32 g_message_box_glyph_delay;
extern s32 g_message_box_digit_divisors[];
extern s32 g_message_box_slot_threads[3];
extern s32 g_message_box_slot_glyph_counters[3];
extern u8 g_message_box_slot_carry_offset[4];
extern u8 g_message_box_slot_prev_width[4];
extern RECT g_message_box_vram_rect;
extern u8 g_message_box_glyph_images[3][8][84];
extern u8 g_message_box_previous_images[3][84];
extern u8 g_message_box_column_scratch[14];
extern u8 g_screen_text_task_phase;
extern SVECTOR g_screen_text_partner_coords;
void message_box_thread(void);
s32 screen_text_is_printing(s32 thread_id);
void screen_text_resume_printing(s32 thread_id);
void message_box_show_with_sound(s32 message_id, s32 sound_id);

/* text images */
extern menu_list_page_t g_screen_text_list_pages[2];
void screen_text_render_rows(u8* font, s16* text_ids, RECT* destination, s32 palette);
void screen_text_kern(u8* text, s16* name, RECT* rect, s32 palette);
s32 screen_text_measure_width(const u8* entry);
void formation_build_roster_names(void);

#endif
