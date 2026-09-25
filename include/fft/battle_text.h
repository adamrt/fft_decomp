#ifndef FFT_BATTLE_TEXT_H
#define FFT_BATTLE_TEXT_H

#include "psx/gpu.h"
#include "psx/types.h"

/* Origin and row geometry used by the EVENT menu text image renderers. */
typedef struct battle_menu_text_image_bounds {
    u16 x;
    u16 y;
    u16 row_stride;
    s16 line_height;
} battle_menu_text_image_bounds_t;

/* Encoded menu-string controls. The 0xe2-0xfd meanings come from the dialog
 * interpreter branches (battle_text_character_handling_thread and its WORLD
 * twin) and, for 0xe7, the menu text-image renderers; codes with an operand
 * consume the following byte. F5/F6 are the {0xF5xx}{0xF6xx} "Alter String"
 * controls. The 0xe0-0xeb substitution codes are text_format_code_e in
 * fft/text.h. */
typedef enum text_control {
    TEXT_EXTENDED_GLYPH_PREFIX_FIRST = 0xd0,
    TEXT_EXTENDED_GLYPH_PREFIX_LAST = 0xdf,
    TEXT_SET_GLYPH_COLOR = 0xe2,     /* operand: glyph colour argument */
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
    TEXT_END_AUTO_CLOSE = 0xff,
} text_control_e;

/* Box type: the 0x70 bits of a DisplayMessage dialog type byte. */
typedef enum dialog_box_type {
    DIALOG_BOX_TYPE_CENTERED = 0x00,
    DIALOG_BOX_TYPE_PORTRAIT = 0x10, /* three lines with a portrait */
    DIALOG_BOX_TYPE_CHECK = 0x20,    /* one to four lines titled "Check" */
    DIALOG_BOX_TYPE_HELP = 0x30,     /* one to four lines titled "Help" */
    DIALOG_BOX_TYPE_TWO_LINE = 0x40,
    DIALOG_BOX_TYPE_EIGHT_LINE = 0x50,
} dialog_box_type_e;

typedef enum menu_decimal_sign_flag {
    MENU_DECIMAL_SIGN_PLUS = 0x0400,
    MENU_DECIMAL_SIGN_MINUS = 0x0800,
} menu_decimal_sign_flag_e;

enum {
    MENU_DECIMAL_FIELD_WIDTH_MASK = 0x000f,
    MENU_DECIMAL_FIELD_WIDTH_CLEAR_MASK = 0xfff0,
    MENU_DECIMAL_SIGN_MASK = 0x0c00,
    MENU_DECIMAL_SIGN_CLEAR_MASK = 0xf3ff,
    MENU_DECIMAL_ZERO_PLACEHOLDER_FLAGS = 0x0804,
};

/* Pen position (not a rectangle) handed to the number glyph renderers;
 * callers point it at menu_text_state_t.origin_x. */
typedef struct battle_rect {
    s16 x; /* 0x00 */
    s16 y; /* 0x02 */
} battle_rect_t;

/* Glyph state of the dialogue text renderer at 0x8016dad4, filled by
 * battle_text_character_handling_thread and drawn by battle_text_draw_dialogue_glyph. */
typedef struct battle_text_glyph_state {
    u16 flags_0;         /* 0x00; bits 0-1 give the glyph's pixel phase */
    u16 _unknown_2;      /* 0x02 */
    u16 character_4;     /* 0x04 */
    u8 _unknown_6[6];    /* 0x06 */
    s16 dialogue_type_c; /* 0x0c */
    s16 palette_e;       /* 0x0e */
} battle_text_glyph_state_t;

typedef char battle_text_glyph_state_size_must_be_0x10[(sizeof(battle_text_glyph_state_t) == 0x10) ? 1 : -1];

extern battle_text_glyph_state_t g_battle_text_typewriter_glyph;

/* Source image used when battle overlays render menu glyphs. */
extern u8* g_battle_menu_glyph_image;
extern const char g_battle_text_decimal_format[];
extern u16 g_battle_text_message_duration_frames;
extern u8* g_battle_text_pointer_backup[32];
extern s32 g_battle_text_pointer_backup_active;
extern s32 g_battle_menu_text_section_offsets[];

/* BATTLE text IDs use their upper five bits to select one of these sections.
 * Overlay loaders replace individual entries as their text becomes active. */
extern u8* g_battle_text_section_pointers[32];

/* text */
void battle_text_build_display_message(s32 offset, s32 flags, s32* out_width);
void battle_text_build_unit_name_list(void);
u8* battle_text_init_entry(u32 text_id);
void battle_text_init_menu_section_pointers(void);
void battle_text_set_message_duration_frames(u16 frames);
void battle_text_relocate_pointer_table(const u32* offsets);
void battle_text_restore_pointer_table(void);
void battle_text_save_pointer_table(void);

void battle_draw_menu_number_glyphs(s32 value, s32 digits, void* resource, battle_rect_t* pos);
void battle_text_draw_large_number_glyphs(s32 value, s32 digits, void* resource, battle_rect_t* pos);

/* Provisional: text pen position and right margin. The caller passes
 * &g_menu_text_state.origin_x, whose stride sits at +0x08. */
typedef struct battle_text_pen {
    u16 x;           /* 0x00 */
    u16 y;           /* 0x02 */
    s32 unknown_04;  /* 0x04 */
    s32 right_limit; /* 0x08 */
} battle_text_pen_t;

void battle_menu_display_text(u32 text_id, s32 image, battle_text_pen_t* origin, u8* text);

void battle_text_character_handling_thread(void);

/* Glyph pixel widths, 0xD0 entries per 0xDx prefix page. */
extern const u8 g_text_glyph_widths[];

void battle_text_configure_sprite_vram(RECT* rect, s16 width, s16 height, SPRT* sprite, s32 palette_row);
void blit_text_glyph(void* text, void* pixels, void* glyph, void* position);
u8* battle_text_advance_cursor_with_separate_backreference_state(s32* remaining_bytes, u8* cursor, u8** return_cursor);
s32 battle_text_count_decimal_digits(s32 value);
void battle_text_draw_dialogue_glyph(s32 pen_x, s32 pen_y, s32 delay);
void battle_text_print_fnt_value_and_next(s32 value);
s32 battle_text_read_packed_nibble(u8* data, s32 index, s32 row, s32 stride);
s32 battle_text_resolve_format_string_id(s32 format_code, s32 value);
extern s32 g_battle_text_awaiting_input;
extern s32 g_battle_text_current_line;
extern u32 g_battle_text_decimal_divisors[];
extern s32 g_battle_text_glyphs_per_wait;             /* glyphs drawn before the typewriter waits */
extern const char g_battle_text_heap_dp_format[];     /* "DP %x\n" */
extern const char g_battle_text_heap_remain_format[]; /* "HEAP REMAIN %x MAX %x\n" */
extern const char g_battle_text_heap_task_format[];   /* "TASK %d %x\n" */
/* Pixel width of each line measured by the last call. */
extern s32 g_battle_text_line_widths[];
extern void* g_battle_text_message_buffer;
extern const char g_battle_text_min_heap_format[];           /* "MIN HEAP %x " */
extern char g_battle_text_reading_character_message[];       /* "reading charactor%d \n" */
extern u8 g_battle_text_slot_row_pitches[];                  /* per-slot source row pitch in bytes */
extern u8 g_battle_text_slot_source_columns[];               /* per-slot source column */
extern s32 g_battle_text_substitution_values[0x80];          /* text variables $00-$7F */
extern u8 g_battle_text_typewriter_column_scratch[14];       /* one-column scratch used by the phase shift */
extern u8 g_battle_text_typewriter_glyph_images[3][8][0x54]; /* eight glyph images per slot */
extern u8 g_battle_text_typewriter_previous_images[3][0x54]; /* previous image per slot */
/* The caller leaves 0x16f in $a1, but the body never reads $a1. */
void battle_text_fill_field_with_terminators(u8* text);

#endif
