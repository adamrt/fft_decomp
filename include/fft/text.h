#ifndef TEXT_H
#define TEXT_H

/* Text shared by BATTLE, WORLD and the overlays: the font, the encoded text format, text sections and ids,
 * substitutions, measuring, drawing text and numbers into images, the dialog window record, the typewriter's glyph
 * slots and the menu text threads. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/screen.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* font */
/* The font, EVENT/FONT.BIN: 2200 glyphs of 14 rows of 10 two-bit pixels. A
 * byte from TEXT_EXTENDED_GLYPH_PREFIX_FIRST selects a page of glyphs and
 * the byte after it the glyph: (prefix & 0xf) * TEXT_GLYPHS_PER_PREFIX + byte. */
enum { TEXT_GLYPH_BYTES = 35, TEXT_GLYPHS_PER_PREFIX = 208 };

/* Glyph pixel widths, TEXT_GLYPHS_PER_PREFIX entries per prefix page. */
extern const u8 g_text_glyph_widths[];
extern u8* g_text_glyph_bitmap_data;
extern glyph_blit_t g_text_small_number_glyph_rect; /* source of the small 6x10 number glyphs */

/* encoded text */
/* Encoded menu-string controls. The 0xe2-0xfd meanings come from the dialog
 * interpreter branches (message_dialog_thread and its WORLD
 * twin) and, for 0xe7, the menu text-image renderers; codes with an operand
 * consume the following byte. F5/F6 are the {0xF5xx}{0xF6xx} "Alter String"
 * controls. The 0xe0-0xeb substitution codes are text_format_code_e. */
typedef enum text_control {
    TEXT_EXTENDED_GLYPH_PREFIX_FIRST = 0xd0,
    TEXT_EXTENDED_GLYPH_PREFIX_LAST = 0xdf,
    TEXT_BACKREFERENCE_PREFIX_FIRST = 0xf0, /* 0xf0-0xf3 start a three-byte back-reference into earlier text */
    TEXT_BACKREFERENCE_PREFIX_LAST = 0xf3,
    TEXT_SET_GLYPH_DELAY = 0xe2,     /* operand: typewriter delay */
    TEXT_SET_PALETTE = 0xe3,         /* operand: text palette */
    TEXT_PRINT_NEXT_VALUE = 0xe4,    /* prints the next substitution value */
    TEXT_PRINT_INDEXED_VALUE = 0xe6, /* operand: substitution value index */
    TEXT_SET_X_POSITION = 0xe7,      /* operand: pen x relative to the text rect */
    TEXT_SET_NUMBER_WIDTH = 0xe8,    /* operand: digit count of the next number */
    TEXT_SET_PORTRAIT = 0xec,        /* operand: portrait, also script variable 0x5a */
    TEXT_PAGE_BREAK = 0xf4,
    TEXT_SET_VARIABLE_BASE = 0xf5, /* operand: script variable for 0xf6 */
    TEXT_STORE_VARIABLE = 0xf6,    /* operand: value stored to that variable */
    TEXT_NEWLINE = 0xf8,
    TEXT_RELEASE_WAITING_THREADS = 0xf9,
    TEXT_SPACE = 0xfa, /* 4-pixel blank glyph */
    TEXT_CHOICE_MARK = 0xfb,
    TEXT_CHOICE_SELECT = 0xfc,
    TEXT_WAIT_FOR_RESUME = 0xfd,
    TEXT_END_WAIT_FOR_CONFIRM = 0xfe,
    TEXT_END_AUTO_CLOSE = 0xff
} text_control_e;

/* Substitution opcodes shared by battle messages and WORLD event text. */
typedef enum text_format_code {
    TEXT_FORMAT_FIRST = 0xe0,
    TEXT_FORMAT_RAMZA_NAME = 0xe0,
    TEXT_FORMAT_UNIT_NAME = 0xe1,
    TEXT_FORMAT_LOCATION_NAME = 0xe5,
    TEXT_FORMAT_ITEM_NAME = 0xe9,
    TEXT_FORMAT_ABILITY_NAME = 0xea,
    TEXT_FORMAT_EXPLICIT_ID = 0xeb
} text_format_code_e;

/* Three-byte backreference: five length bits and a base-254 backward offset. */
typedef struct text_backref {
    u8 length_high : 2;
    u8 opcode : 6;
    u8 offset_high : 4;
    u8 _unused_01 : 1;
    u8 length_low : 3;
    u8 offset_low;
} text_backref_t;
typedef char text_backref_size_must_be_3[sizeof(text_backref_t) == 3 ? 1 : -1];

/* Text-stream cursor state used by measurement and menu rendering. The
 * measurers use scratchpad 0x1f800000; menu rendering uses 0x1f800200. */
typedef struct text_reader_state {
    s32 command;             /* 0x00; current text byte */
    s32 column;              /* 0x04; characters on the current line */
    s32 max_column;          /* 0x08; widest line so far */
    s32 row;                 /* 0x0c; completed lines */
    s32 value;               /* 0x10; substitution value / text id */
    s32 scratch;             /* 0x14; intermediate byte-count / back-offset */
    s32 remaining_bytes;     /* 0x18; bytes left before returning to return_cursor */
    const u8* return_cursor; /* 0x1c; cursor to resume at, -1 when none */
    u8 _unused_20[4];
    s32* value_cursor; /* 0x24; next substitution value */
    s32 pending_width; /* 0x28; explicit digit width from 0xE8, 0xFFFF when none */
    s32 glyph_page;    /* 0x2c; pending 0xDx page-prefix byte, 0 otherwise (text_measure_pixels) */
} text_reader_state_t;

const u8* text_advance_cursor_with_backreferences(text_reader_state_t* state, const u8* cursor);

/* The menu renderer's own copy of the same cursor helper. */
const u8* menu_text_advance_cursor_with_backreferences(text_reader_state_t* state, const u8* cursor);

/* The form the script interpreter and the per-program dialog threads use, with the back-reference state in
 * separate variables. */
const u8* text_advance_cursor_with_separate_backreference_state(
    s32* remaining_bytes, const u8* cursor, const u8** return_cursor);

/* text sections */
/* A text id selects a section in its upper five bits and an entry in the lower eleven. */
enum {
    TEXT_SECTION_COUNT = 32,
    TEXT_ID_SECTION_MASK = 0xf800,
    TEXT_ID_SECTION_SHIFT = 11,
    TEXT_ID_ENTRY_MASK = 0x07ff
};

/* Text ids of the first entry of the text sections that text resolution and unit-name loading use.
 * Unit-name classes reserve one 0x100-entry page each. */
typedef enum text_id_base {
    TEXT_ID_SKILLSET_NAME_BASE = 0x1000,
    TEXT_ID_JOB_NAME_BASE = 0x3000,
    TEXT_ID_ITEM_NAME_BASE = 0x3800,
    TEXT_ID_UNIT_NAME_BASE = 0x4000,
    TEXT_ID_UNIT_NAME_SPECIAL_BASE = TEXT_ID_UNIT_NAME_BASE,
    TEXT_ID_UNIT_NAME_GENERIC_MALE_BASE = 0x4100,
    TEXT_ID_UNIT_NAME_GENERIC_FEMALE_BASE = 0x4200,
    TEXT_ID_UNIT_NAME_GENERIC_MONSTER_BASE = 0x4300,
    TEXT_ID_ABILITY_NAME_BASE = 0x7000,
    /* By location_id_e. ATTACK binds its deployment messages to this
     * section instead. */
    TEXT_ID_LOCATION_NAME_BASE = 0x9000,
    TEXT_ID_SKILLSET_NAME_SOURCE_BASE = 0xb000, /* section 22, which SMALL also binds as section 2 */
    /* The sections of EVENT/WORLD.LZW that only the world map uses, each
     * indexed by the id its name gives unless noted. */
    TEXT_ID_WORLD_HELP_MESSAGE_BASE = 0x5800,
    TEXT_ID_COMPLETED_PROPOSITION_NAME_BASE = 0x6000, /* the Brave Story's Job list, by proposition */
    TEXT_ID_PROPOSITION_REWARD_NAME_BASE = 0x7800,
    TEXT_ID_TIP_NAME_BASE = 0x8000,
    TEXT_ID_WORLD_MESSAGE_BASE = 0x8800, /* the block of WORLD/WLDMES.BIN that is loaded */
    TEXT_ID_FACT_NAME_BASE = 0x9800,
    TEXT_ID_MAP_NAME_BASE = 0xa800,   /* by battle_map_id_e less one */
    TEXT_ID_WORLD_MENU_BASE = 0xb800, /* world_map_text_id_e */
    TEXT_ID_TUTORIAL_NAME_BASE = 0xc000,
    TEXT_ID_RUMOR_NAME_BASE = 0xc800,
    TEXT_ID_PROPOSITION_NAME_BASE = 0xd000,
    TEXT_ID_UNEXPLORED_LAND_NAME_BASE = 0xd800,
    TEXT_ID_TREASURE_NAME_BASE = 0xe000,
    TEXT_ID_STORY_EVENT_NAME_BASE = 0xe800,        /* the Brave Story's Record list */
    TEXT_ID_PERSON_PROFILE_NAME_BASE = 0xf000,     /* by person * 16 + profile */
    TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE = 0xf800 /* by proposition_fields_t::name_text less one */
} text_id_base_e;

/* Reward names in the TEXT_ID_PROPOSITION_REWARD_NAME_BASE section: the random bonuses from the base,
 * then the no-reward name, the unexplored lands and the treasures, each by reward_index. */
enum {
    TEXT_ID_REWARD_NONE = TEXT_ID_PROPOSITION_REWARD_NAME_BASE + 6,
    TEXT_ID_REWARD_LAND_FIRST = TEXT_ID_PROPOSITION_REWARD_NAME_BASE + 0x1b,
    TEXT_ID_REWARD_TREASURE_FIRST = TEXT_ID_PROPOSITION_REWARD_NAME_BASE + 0x2e
};

enum { TEXT_ID_FORMATION_TEXT_BASE = SCREEN_TEXT_SECTION_FORMATION_TEXT << TEXT_ID_SECTION_SHIFT };

/* Index into g_text_section_pointers: a text id's top five bits. Only the sections the code binds or reads by a
 * fixed role are named. A screen rebinds a slot to its own text while it is up: help_text_section_e,
 * screen_text_section_e and after_battle_text_section_e map those bindings. */
typedef enum text_section {
    TEXT_SECTION_EVENT_MESSAGES = 0,      /* the loaded event's text; BUNIT binds its menu text here */
    TEXT_SECTION_SKILLSET_NAMES_COPY = 2, /* TEXT_ID_SKILLSET_NAME_BASE; a copy of section 22 */
    TEXT_SECTION_JOB_NAMES = 6,           /* TEXT_ID_JOB_NAME_BASE */
    TEXT_SECTION_ITEM_NAMES = 7,          /* TEXT_ID_ITEM_NAME_BASE */
    TEXT_SECTION_ROSTER_UNIT_NAMES = 8,   /* TEXT_ID_UNIT_NAME_BASE */
    TEXT_SECTION_UNIT_NAMES = 9,
    TEXT_SECTION_PARTY_UNIT_NAMES = 11, /* names by party slot, which ATTACK writes for the deployment screen */
    TEXT_SECTION_ABILITY_NAMES = 14,    /* TEXT_ID_ABILITY_NAME_BASE */
    TEXT_SECTION_MESSAGES = 17,
    TEXT_SECTION_LOCATION_NAMES = 18,
    TEXT_SECTION_UNIT_MESSAGE = 21,         /* SPELL.MES while the spell quote thread runs */
    TEXT_SECTION_SKILLSET_NAMES = 22,       /* TEXT_ID_SKILLSET_NAME_SOURCE_BASE */
    TEXT_SECTION_FORMATION_MESSAGES = 24,   /* WORLD formation and name-entry messages; CARD binds its own copy */
    TEXT_SECTION_COMPOSED_TEXT = 25,        /* a buffer the screen fills: a sort or reorder list, the entered name */
    TEXT_SECTION_DEPLOYMENT_UNIT_HELP = 26, /* ATTACK: eleven help strings per party slot */
    TEXT_SECTION_OVERLAY_SCREEN = 27,       /* each overlay screen binds its own text here */
    TEXT_SECTION_AT_LIST = 31               /* the AT list's text, built when the list opens */
} text_section_e;

extern u32 g_text_section_offsets[TEXT_SECTION_COUNT]; /* the text file's table; the text follows it */
extern u8* g_text_section_pointers[TEXT_SECTION_COUNT];

/* The text file an overlay loaded. */
extern u8 g_text_data[];

void text_init_section_pointers(void);
void text_save_section_pointers(void);
void text_restore_section_pointers(void);

/* MAIN selects the BATTLE address of text_find_entry through this existing alias. */
u8* battle_text_find_entry(u32 text_id);
u8* text_find_entry(u32 text_id);
const u8* text_skip_to_entry(const u8* text, s32 entry_index);
const u8* menu_text_find_entry_by_index(const u8* data, s16 count, s32 unused_2);

/* Bind writable sections of a text bank beginning with its offset table. */
void text_relocate_pointer_table(u32* offsets);

/* battle name text */
/* SMALL.OUT's entrypoint: bind its battle name text and clear the deployment unit limit. */
void small_entrypoint(void);

/* substitutions */
/* Player-name substitution, padded to 20 bytes with TEXT_END_WAIT_FOR_CONFIRM. */
extern u8 g_text_player_name_buffer[20];

/* Values $00-$1F; indexed substitutions can read past the named 32 entries. */
extern s32 g_text_substitution_values[32];

void text_set_substitution_values_7e_7f(s32 x, s32 y, s32 width, s32 height);

/* The text id a substitution code (text_format_code_e) prints for value. */
s32 text_resolve_format_string_id(s32 format_code, s32 value);

/* Packs the names of the battle units into TEXT_SECTION_ROSTER_UNIT_NAMES. */
void text_build_unit_name_list(void);

/* measuring */
/* Width and line count returned by text measurement. Width is in pixels for
 * text_measure_pixels and character columns for text_measure. */
typedef struct text_dimensions {
    s16 width;
    s16 rows;
} text_dimensions_t;
typedef char text_dimensions_size_must_be_4[sizeof(text_dimensions_t) == 4 ? 1 : -1];

/* Pixel width of each line from the last text_measure_pixels call. */
extern s32 g_text_line_widths[];

void text_measure(s16* columns, s16* rows, const u8* text);
void text_measure_pixels(s16* width, s16* rows, const u8* text);

/* text images */
/* Pen of menu_text_render_glyph_to_4bpp_image: x in pixels (two per byte), y in rows, and the image's row stride in
 * 16-bit values. */
typedef struct text_image_position {
    s16 x;          /* 0x00 */
    s16 y;          /* 0x02 */
    s16 row_stride; /* 0x04 */
} text_image_position_t;
typedef char text_image_position_size_must_be_6[(sizeof(text_image_position_t) == 6) ? 1 : -1];

/* Origin and row geometry of the image menu_text_render_encoded_ids_to_image draws into. */
typedef struct text_image_bounds {
    u16 x;
    u16 y;
    u16 row_stride;
    s16 line_height;
} text_image_bounds_t;

/* BATTLE and WORLD each define this menu-window pen, independently of the script-variable pointer before it. */
extern glyph_blit_t g_menu_window_text_pen;
extern glyph_blit_t g_menu_text_pen;

/* Visible line range [first_line, line_limit); BATTLE and WORLD each define these. */
extern s32 g_menu_text_first_line;
extern s32 g_menu_text_line_limit;
extern s32 g_text_glyph_first_row;
extern s32 g_text_glyph_row_limit;

/* Added to glyph palette indices while menu text is drawn. */
extern s32 g_menu_text_palette_offset;

/* Text image buffer of the options and AT list screens and of menu_redraw_text_page_on_scroll. */
extern u8 g_menu_render_buffer[];

/* The source and destination rectangles of the next glyph blit. */
extern glyph_blit_t g_text_glyph_source;
extern glyph_blit_t g_text_glyph_dest;

void menu_set_text_origin(s32 x, s32 y);
void menu_clear_render_buffer(void* buffer, u32 bytes);
void text_set_blit_source_rect(s16 source_x, s16 source_y, s16 width, s16 height);
void text_set_blit_destination(s16 destination_x, s16 destination_y);
void text_render_glyph_to_4bpp_image(const u8* glyph_bitmap, u8* image, glyph_blit_t* origin, s32 palette_offset);
void text_blit_glyph(const u8* source, u8* pixels, const glyph_blit_t* glyph, const glyph_blit_t* position);
s32 menu_text_render_glyph_to_4bpp_image(s32 glyph_id, u8* image, const text_image_position_t* position, s32 style);
void menu_display_text_string(u8* image, glyph_blit_t* origin, const u8* text);
void menu_display_text_entry(s32 text_id, u8* image, glyph_blit_t* origin);
void menu_display_text_entry_past_choice_marks(s32 text_id, u8* image, glyph_blit_t* origin);
void menu_display_text(s32 text_id, u8* image, glyph_blit_t* origin, const u8* text);
void menu_draw_text_columns_narrow(menu_entry_t* entry, s32* row_offset, u8* buffer);
void menu_text_render_encoded_ids_to_image(u8* image, text_image_bounds_t* rect, s32 unused, s32 max_chars, u8* font,
    s16* ids, s32 count, s16 glyph, s32 unused_flags);
void text_render_id_rows_to_vram(u8* text_table, s16* list, RECT* rect, s32 style);
void text_concatenate_entries(const u8* text_table, u8* out, s16* list, s32 separate);

/* Allocates text image VRAM for the sprite or quad and gives it the normal menu CLUT plus clut_offset rows. */
void text_configure_sprite_vram(RECT* rect, s32 width, s32 height, void* packet, s32 clut_offset);

/* numbers */
/* Sign flags read by the decimal formatter and large-glyph renderer. */
typedef enum menu_decimal_sign_flag {
    MENU_DECIMAL_SIGN_PLUS = 0x0400,
    MENU_DECIMAL_SIGN_MINUS = 0x0800
} menu_decimal_sign_flag_e;

/* The other format flags menu_text_render_decimal_value tests. */
typedef enum menu_decimal_format_flag {
    MENU_DECIMAL_DOTS_PREFIX = 0x0100,        /* a two-row dotted glyph before the number */
    MENU_DECIMAL_WIDEN_FOR_100 = 0x0200,      /* a value of 100 is drawn with three digits */
    MENU_DECIMAL_THREE_COLUMN_FIELD = 0x2000, /* right-aligned in three digit columns */
    MENU_DECIMAL_LEADING_ZEROS = 0x4000,      /* every digit of the count is drawn */
    MENU_DECIMAL_SLASH_PREFIX = 0x8000        /* a "/" glyph before the number */
} menu_decimal_format_flag_e;

enum {
    MENU_DECIMAL_FIELD_WIDTH_MASK = 0x000f,
    MENU_DECIMAL_FIELD_WIDTH_CLEAR_MASK = 0xfff0,
    MENU_DECIMAL_SIGN_MASK = 0x0c00,
    MENU_DECIMAL_SIGN_CLEAR_MASK = 0xf3ff,
    MENU_DECIMAL_ZERO_PLACEHOLDER_FLAGS = 0x0804
};

/* Small-glyph number formats use a digit count in the low byte and these flags in the high byte.
 * Bit 0x4000 has no standalone effect in digit mode. */
typedef enum small_number_format_flag {
    SMALL_NUMBER_DOTS_PREFIX = 0x0100,
    SMALL_NUMBER_WIDEN_FOR_100 = 0x0200,
    SMALL_NUMBER_ALIGN_TWO_DIGITS = 0x0400, /* values other than 100: two digits, shifted six pixels right */
    SMALL_NUMBER_DASH = 0x0800,             /* BATTLE: dash prefix; WORLD: replace each digit with a dash */
    SMALL_NUMBER_SLASH_PREFIX = 0x1000,
    SMALL_NUMBER_PERCENT_SUFFIX = 0x2000,
    SMALL_NUMBER_SLASH_PREFIX_LEFT = 0x8000 /* slash two pixels left of the normal prefix */
} small_number_format_flag_e;

/* These complete high-byte modes replace the number with three placeholders.
 * Their low byte is not a digit count; bit 0 adds a slash prefix. */
enum {
    SMALL_NUMBER_PLACEHOLDER_DASHES = 0x0c00,
    SMALL_NUMBER_PLACEHOLDER_QUESTIONS = 0x0e00,
    SMALL_NUMBER_PLACEHOLDER_SLASH = 0x01,
    SMALL_NUMBER_DIGIT_COUNT_MASK = 0x00ff,
    SMALL_NUMBER_FORMAT_MASK = 0xff00
};

/* Destination rectangle and source cell for a decimal digit. */
typedef struct menu_digit_glyph {
    RECT rect;
    s16 u;
    s16 v;
    s16 w;
    u16 h;
} menu_digit_glyph_t;
typedef char menu_digit_glyph_size_must_be_16[sizeof(menu_digit_glyph_t) == 16 ? 1 : -1];

extern u32 g_text_decimal_divisors[];

s32 text_count_decimal_digits(s32 value);
void menu_draw_large_number_glyphs(s32 value, s32 digits, u8* pixels, glyph_blit_t* pos);

/* The battle ability lists draw this pair in the number column instead of a value of 0 or 1 when the ability's
 * skill_ct is 0 or 0xff. */
void menu_draw_stacked_glyph_pair(u8* pixels, glyph_blit_t* position);
void menu_display_number_entry(s32 value, u8 digit_count, u8* image, glyph_blit_t* origin);
void menu_draw_numeric_display_entries(
    u8* buffer, const menu_number_entry_t* entries, glyph_blit_t* position, s32 count);
void menu_text_render_decimal_entry_list(u8* pixels, const menu_number_entry_t* entries, glyph_blit_t* out, s32 count);
void menu_text_render_signed_decimal_entries(u8* pixels, menu_number_entry_t* entries, glyph_blit_t* out, s32 count);

/* BATTLE and WORLD each have their own body. */
void text_draw_small_number_glyphs(s32 value, s32 format, u8* pixels, glyph_blit_t* pos);

/* debug font output */
/* Prints value and value + 1 to the debug font stream. */
void text_print_fnt_value_and_next(s32 value);

/* dialog windows */
/* Box type: the 0x70 bits of a DisplayMessage dialog type byte. */
typedef enum dialog_box_type {
    DIALOG_BOX_TYPE_CENTERED = 0x00,
    DIALOG_BOX_TYPE_PORTRAIT = 0x10, /* three lines with a portrait */
    DIALOG_BOX_TYPE_CHECK = 0x20,    /* one to four lines titled "Check" */
    DIALOG_BOX_TYPE_HELP = 0x30,     /* one to four lines titled "Help" */
    DIALOG_BOX_TYPE_TWO_LINE = 0x40,
    DIALOG_BOX_TYPE_EIGHT_LINE = 0x50
} dialog_box_type_e;

/* The low two dialog-type bits place the window vertically. */
typedef enum dialog_position {
    DIALOG_POSITION_AUTO = 0, /* portrait tail: choose above/below from projected speakers */
    DIALOG_POSITION_TOP = 1,
    DIALOG_POSITION_BOTTOM = 2,
    DIALOG_POSITION_CENTER = 3
} dialog_position_e;

/* Fields of a DisplayMessage dialog type byte
 * (text_dialog_record_t::dialog_type). */
enum {
    DIALOG_TYPE_POSITION_MASK = 0x03,
    DIALOG_TYPE_OPTIONS_MASK = 0x0c,
    DIALOG_TYPE_BOX_TYPE_MASK = 0x70, /* dialog_box_type_e */
    /* The window stays open after its text: ChangeDialog resumes it with a new text id, and
     * text id 0xFFFF closes it. */
    DIALOG_TYPE_KEEP_OPEN = 0x80
};

/* The DIALOG_TYPE_OPTIONS_MASK field of a portrait box, as
 * menu_build_window_image and the text thread read it. 0x0c is tested with
 * the Help box only and has no name here. */
typedef enum dialog_options {
    DIALOG_OPTIONS_TAIL = 0x00,           /* a tail pointing at the speaker */
    DIALOG_OPTIONS_ALTERNATE_TAIL = 0x04, /* the second tail graphic, 16 rows tall */
    DIALOG_OPTIONS_NO_TAIL = 0x08         /* plain frame, not placed at the speaker */
} dialog_options_e;

/* Text window record of the dialog and message-box threads, rendered by
 * message_render_record_image and WORLD's message_box_render_text_image: they build an
 * image buffer (the window builder when box_type is 0x10), draw `text` into
 * it and upload `rect`. */
typedef struct text_dialog_record {
    u8* buffer;        /* 0x00 */
    u16 width;         /* 0x04 */
    u16 height;        /* 0x06 */
    RECT rect;         /* 0x08 */
    s32 text;          /* 0x10 */
    u16 color;         /* 0x14 */
    u16 dialog_type;   /* 0x16: DisplayMessage Dialog Type byte */
    u16 box_type;      /* 0x18: dialog_type & 0x70 */
    u16 position;      /* 0x1a */
    u16 options;       /* 0x1c: dialog_type & 0x0c (dialog_options_e) */
    u16 keep_open;     /* 0x1e: dialog_type & DIALOG_TYPE_KEEP_OPEN */
    u16 line;          /* 0x20 */
    u16 first_line;    /* 0x22 */
    u16 last_line;     /* 0x24 */
    u16 arrow_x;       /* 0x26 */
    u16 arrow_y;       /* 0x28 */
    s16 _unknown_2a;   /* 0x2a */
    s16 mark;          /* 0x2c: measured width, later the 0xFB cursor mark */
    s16 lines;         /* 0x2e */
    s16 tail_offset;   /* 0x30: dialog tail offset from the window center */
    u8 _padding_32[2]; /* 0x32; aligns x */
    s32 x;             /* 0x34 */
    s32 y;             /* 0x38 */
    s32 portrait;      /* 0x3c */
    u16 origin_x;      /* 0x40 */
    u16 origin_y;      /* 0x42 */
    s32 arrow_offset;  /* 0x44: DisplayMessage arrow position, added to offset */
    s32 x_offset;      /* 0x48: DisplayMessage X coordinate, added to x */
    s32 y_offset;      /* 0x4c: DisplayMessage Y coordinate, added to y */
} text_dialog_record_t;

/* Task words of a dialog thread, which the DisplayMessage instruction fills
 * before the thread runs; its other starters set only the y offset. */
enum {
    DIALOG_THREAD_WORD_X_OFFSET = 2,
    DIALOG_THREAD_WORD_Y_OFFSET = 3,
    DIALOG_THREAD_WORD_ARROW_OFFSET = 4,
    DIALOG_THREAD_WORD_WINDOW_OPTIONS = 5, /* low nibble picks the opening tween; a high nibble mirrors the tail */
    DIALOG_THREAD_WORD_TEXT_WIDTH = 6      /* nonzero overrides the measured width */
};

extern s32 g_text_current_line;

/* typewriter glyphs */
/* One pending text glyph: g_message_typewriter_glyph, which message_dialog_thread fills and
 * message_draw_glyph_with_delay draws, and WORLD's message-box glyph g_message_box_glyph
 * (message_box_thread). The typewriter's menu text state and VRAM rectangle follow it in memory but
 * are separate globals. */
typedef struct text_glyph_state {
    u16 x;            /* 0x00: pen x; the low two bits select the sub-byte pixel phase */
    s16 y;            /* 0x02: pen y */
    u16 glyph;        /* 0x04: character code; 0xfa is the blank that skips the upload */
    u8 _unused_06[6]; /* 0x06 */
    s16 mode;         /* 0x0c: 0 blanks the image, 0x10 draws it; the message box also accepts 0x20 */
    s16 palette;      /* 0x0e */
} text_glyph_state_t;
typedef char text_glyph_state_size_must_be_16[(sizeof(text_glyph_state_t) == 16) ? 1 : -1];

/* The three per-thread staging slots the typewriter draws glyphs through. */
extern s32 g_text_slot_owner_thread_ids[3];
extern s32 g_text_slot_glyph_counts[3];

/* text threads */
extern s32 g_text_selection_id;
extern u8 g_text_thread_running_state;

/* Starts the text thread on thread_id only when its scheduler slot is idle. */
void menu_text_start_thread_if_idle(s32 thread_id, s32 first, s32 second, s32 third);

/* Starts the text thread when no text is pending and returns 1; returns 0 once the pending text matches key and
 * has finished, otherwise the running status. */
s32 menu_text_start_or_poll_thread(s32 thread_id, s32 parameter_0, s32 key, s32 parameter_2, s32 y_offset);

#endif
