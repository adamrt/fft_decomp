#ifndef MENU_SCRIPT_H
#define MENU_SCRIPT_H

/* The menu script: byte-aligned display instructions that WORLD, BUNIT, EQUIP and JOBSTTS run through their
 * handler tables, the row callbacks the instructions call, and the scrollable lists the list instruction
 * draws. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "psx/types.h"

/* opcodes */
/* Opcode byte of a menu script instruction: the index of its handler in g_menu_opcode_handlers, the table WORLD,
 * BUNIT, EQUIP and JOBSTTS each link. The handler for value NN is named menu_opcode_NN_<name> (shared menu
 * library) or <program>_menu_opcode_NN_<name> (a program's own copy); CARD has no table and calls its few
 * drawing routines directly. Opcode 8 has no handler anywhere. BUNIT also leaves 3 and 7 empty, EQUIP leaves
 * 0x19-0x1b empty, and JOBSTTS leaves 3, 6, 7, 0x0b and 0x16-0x1b empty. Each module ends a stream with its
 * own opcode, the first one past its table (SCREEN_SCRIPT_END, BUNIT_CMD_END, EQUIP_CMD_END,
 * JOBSTTS_CMD_END). */
typedef enum menu_script_opcode {
    MENU_SCRIPT_OP_WINDOW_FRAME = 0x00,
    MENU_SCRIPT_OP_IF_GROUP = 0x01,      /* run the next group when a row callback returns nonzero */
    MENU_SCRIPT_OP_IF_ELSE_GROUP = 0x02, /* run one of the next two groups */
    MENU_SCRIPT_OP_TILE = 0x03,
    MENU_SCRIPT_OP_UNCOLORED_QUAD = 0x04, /* a textured quad drawn unshaded */
    MENU_SCRIPT_OP_COLORED_QUAD = 0x05,   /* the same handler, shaded with the current draw color */
    MENU_SCRIPT_OP_CALLBACK_QUAD = 0x06,  /* a textured quad whose source cell a row callback picks */
    MENU_SCRIPT_OP_LINE = 0x07,
    MENU_SCRIPT_OP_FIXED_CLUT = 0x09,
    MENU_SCRIPT_OP_CLUT = 0x0a,
    MENU_SCRIPT_OP_DRAW_COLOR = 0x0b,
    MENU_SCRIPT_OP_TEXTURE_PAGE = 0x0c,
    MENU_SCRIPT_OP_NUMBER = 0x0d,
    MENU_SCRIPT_OP_SEMI_TRANS = 0x0e,
    MENU_SCRIPT_OP_DRAW_PRIORITY = 0x0f,
    MENU_SCRIPT_OP_SCROLLABLE_LIST = 0x10, /* the list geometry menu_draw_scrollable_list reads */
    MENU_SCRIPT_OP_TILED_RECTANGLE = 0x11,
    MENU_SCRIPT_OP_OPENING_WINDOW_FRAME = 0x12,
    MENU_SCRIPT_OP_CLEAR_DRAW_AREA = 0x13,
    MENU_SCRIPT_OP_DRAW_AREA = 0x14,
    MENU_SCRIPT_OP_REPEAT_DRAW_AREA = 0x15,
    MENU_SCRIPT_OP_ADVANCE_WINDOW_SCALE = 0x16,
    MENU_SCRIPT_OP_RIGHT_ALIGNED_NUMBER = 0x17,
    MENU_SCRIPT_OP_BLEND_DRAW_MODE = 0x18,
    /* Variants of NUMBER that the WORLD and BUNIT number handlers tell apart from its plain form. */
    MENU_SCRIPT_OP_NUMBER_TINTED_NO_LEADING_ZEROS = 0x19, /* sprite color; leading zeros not drawn */
    MENU_SCRIPT_OP_NUMBER_DIM_LEADING_ZEROS = 0x1a,       /* leading zeros in the disabled clut */
    MENU_SCRIPT_OP_NUMBER_TINTED = 0x1b,                  /* sprite color */
} menu_script_opcode_e;

/* instructions */
/* Seven-byte window command in the menu script streams. The length field
 * advances to the next command, which may have a different size. */
typedef struct menu_window_command {
    u8 opcode;
    u8 length;
    u8 rectangle_source;
    u8 x;
    u8 y;
    u8 width;
    u8 height;
} menu_window_command_t;

/* Flags a menu-script number getter (a row callback) returns above its
 * value, read by every program's NUMBER handler (*_menu_opcode_0d_draw_formatted_number). */
typedef enum menu_number_value_flags {
    MENU_NUMBER_VALUE_DASHES = 0x20000000,   /* a row of dashes instead of the digits */
    MENU_NUMBER_VALUE_DISABLED = 0x40000000, /* drawn with the disabled number clut */
} menu_number_value_flags_e;

/* Ten-byte menu-script "draw number" command: an
 * opcode/length prefix followed by the value getter (a row-callback index),
 * layout and byte position operands. Every program's copy of the
 * formatted-number command advances by `length`. */
typedef struct menu_draw_number_command {
    u8 opcode;
    u8 length;
    u8 style;
    u8 getter;
    u8 width;
    u8 spacing;
    u8 alternate_palette;
    u8 x;
    u8 y;
    u8 value;
} menu_draw_number_command_t;

/* Variable-length list command (0x10) in the menu display stream: the layout
 * screen_load_list_layout reads and the placement
 * each program's *_menu_opcode_10_draw_scrollable_list draws with. */
typedef struct menu_list_record {
    u8 opcode;          /* 0x00: 0x10 = list layout */
    u8 length;          /* 0x01: byte length of this record */
    u8 show_cursor;     /* 0x02 */
    u8 _unused_03;      /* 0x03 */
    u8 skip_count;      /* 0x04: row records that follow (g_screen_list_row_record_count) */
    u8 row_height;      /* 0x05 */
    u8 visible_rows;    /* 0x06 */
    u8 x;               /* 0x07 */
    u8 y;               /* 0x08 */
    u8 width_a;         /* 0x09 */
    u8 width_b;         /* 0x0a */
    u8 cursor_x_offset; /* 0x0b */
    u8 arrow_up_y;      /* 0x0c */
    u8 arrow_down_y;    /* 0x0d */
} menu_list_record_t;

/* Byte-aligned commands in the variable-length menu display stream. */
typedef struct menu_sprite_command {
    u8 opcode;
    u8 length;
    u8 _unused_02;
    menu_sprite_cell_t sprite;
} menu_sprite_command_t;
typedef char menu_sprite_command_size_must_be_9[sizeof(menu_sprite_command_t) == 9 ? 1 : -1];

typedef struct menu_callback_sprite_command {
    u8 opcode;
    u8 length;
    u8 callback_index;
    u8 row;
    u8 x;
    u8 y;
} menu_callback_sprite_command_t;

typedef struct menu_tile_command {
    u8 opcode;
    u8 length;
    u8 x;
    u8 y;
    u8 width;
    u8 height;
    rgb24_t color;
} menu_tile_command_t;
typedef char menu_tile_command_size_must_be_9[sizeof(menu_tile_command_t) == 9 ? 1 : -1];

/* LINE_F2 uses endpoint coordinates rather than a rectangle's extents. */
typedef struct menu_line_command {
    u8 opcode;
    u8 length;
    u8 x0;
    u8 y0;
    u8 x1;
    u8 y1;
    rgb24_t color;
} menu_line_command_t;
typedef char menu_line_command_size_must_be_9[sizeof(menu_line_command_t) == 9 ? 1 : -1];

typedef struct menu_draw_area_command {
    u8 opcode;
    u8 length;
    u8 x;
    u8 y;
    u8 width;
    u8 height;
} menu_draw_area_command_t;

typedef struct menu_sprite_color_command {
    u8 opcode;
    u8 length;
    rgb24_t normal;
    rgb24_t alternate;
} menu_sprite_color_command_t;
typedef char menu_sprite_color_command_size_must_be_8[sizeof(menu_sprite_color_command_t) == 8 ? 1 : -1];

/* Four-byte menu state commands, including draw priority and blend state. */
typedef struct menu_draw_state_command {
    u8 opcode;
    u8 length;
    u8 _unused_02;
    u8 value;
} menu_draw_state_command_t;

typedef struct menu_fixed_clut_command {
    u8 opcode;
    u8 length;
    u8 y_high;
    u8 x_div_16;
    u8 y_low;
} menu_fixed_clut_command_t;

typedef struct menu_texture_page_command {
    u8 opcode;
    u8 length;
    u8 y_blend; /* bits 0-1: y/256; bits 4-5: blend rate */
    u8 x_div_16;
    u8 depth;
} menu_texture_page_command_t;

/* A view of the current byte-aligned instruction, not a fixed-stride array.
 * Commands advance by their encoded byte length; opcode 0x16 and the module's
 * terminator are single bytes without a length operand. */
typedef union menu_instruction {
    struct {
        u8 opcode;
        u8 length;
    } header;
    struct {
        u8 opcode;
        u8 length;
        u8 _unused_02;
        u8 predicate;
        u8 count;
        u8 value;
    } conditional;
    struct {
        u8 opcode;
        u8 length;
        u8 _unused_02;
        u8 predicate;
        u8 count_true;
        u8 count_false;
        u8 value;
    } if_else;
    menu_window_command_t window;
    menu_draw_number_command_t number;
    menu_list_record_t list;
    menu_sprite_command_t sprite;
    menu_callback_sprite_command_t callback_sprite;
    menu_tile_command_t tile;
    menu_line_command_t line;
    menu_draw_area_command_t draw_area;
    menu_sprite_color_command_t sprite_color;
    menu_fixed_clut_command_t fixed_clut;
    struct {
        u8 opcode;
        u8 length;
        u8 normal_y_high;
        u8 normal_x_div_16;
        u8 normal_y_low;
        u8 alternate_y_high;
        u8 alternate_x_div_16;
        u8 alternate_y_low;
    } clut;
    menu_texture_page_command_t texture_page;
    menu_draw_state_command_t draw_state;
    struct {
        u8 opcode;
        u8 length;
        u8 blend_rate;
    } blend;
} menu_instruction_t;
typedef char menu_instruction_size_must_be_14[sizeof(menu_instruction_t) == 14 ? 1 : -1];

/* callbacks */
/* The display opcode selects the row callback's result: an icon source
 * for opcode 0x06, or an integer for predicates and number renderers. */
typedef union menu_callback {
    s32 (*value)(s32 row);
    menu_icon_source_t* (*icon)(s32 row);
} menu_callback_t;
typedef char menu_callback_size_must_be_4[sizeof(menu_callback_t) == 4 ? 1 : -1];

/* running scripts */
/* menu_script_run calls the handler of each instruction's opcode until the module's terminator; each handler
 * returns the next instruction. */

/* Input the stream's instructions see; a dimmed palette or a zooming window withholds it. */
extern u32 g_menu_script_input;
extern menu_instruction_t* (*g_menu_opcode_handlers[])(menu_instruction_t* instruction);

/* JOBSTTS builds its own body. */
void menu_script_run(menu_instruction_t* instruction, s32 flags);
void menu_run_script_with_palette_mode(menu_instruction_t* script, s32 flags, s32 event_mode);
menu_instruction_t* menu_script_skip_group(menu_instruction_t* command);

static inline menu_instruction_t* menu_next_instruction(menu_instruction_t* instruction) {
    return (menu_instruction_t*)((u8*)instruction + instruction->header.length);
}

static inline const menu_instruction_t* menu_next_const_instruction(const menu_instruction_t* instruction) {
    return (const menu_instruction_t*)((const u8*)instruction + instruction->header.length);
}

/* opcode handlers */
menu_instruction_t* menu_opcode_00_draw_window_frame(menu_instruction_t* command);
menu_instruction_t* menu_opcode_01_if_group(menu_instruction_t* command);
menu_instruction_t* menu_opcode_02_if_else_group(menu_instruction_t* command);
menu_instruction_t* menu_opcode_04_draw_textured_quad(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_06_draw_callback_quad(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_09_set_fixed_clut(menu_instruction_t* command);
menu_instruction_t* menu_opcode_0a_set_clut(menu_instruction_t* command);
menu_instruction_t* menu_opcode_0b_set_draw_color(menu_instruction_t* command);
menu_instruction_t* menu_opcode_0c_set_texture_page(menu_instruction_t* command);
menu_instruction_t* menu_opcode_0e_set_semi_trans(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_0f_set_draw_priority(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_11_draw_background_tiles(menu_instruction_t* instruction);
menu_window_command_t* menu_opcode_11_draw_tiled_rectangle(menu_window_command_t* command);
menu_instruction_t* menu_opcode_12_draw_opening_window_frame(menu_instruction_t* instruction);
/* JOBSTTS builds its own body. */
menu_instruction_t* menu_opcode_13_clear_temporary_draw_area(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_14_set_draw_area_if_unlocked(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_15_repeat_draw_area(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_16_advance_window_scale_step(menu_instruction_t* instruction);
menu_instruction_t* menu_opcode_18_add_blend_draw_mode(menu_instruction_t* instruction);

/* drawing commands */
extern menu_callback_t g_menu_row_callbacks[16];
extern menu_sprite_command_t g_menu_script_quad_command;
extern menu_window_command_t g_menu_window_interior_command;
/* The JOBSTTS and CARD window-frame handlers' copy of g_menu_window_interior_command. */
extern menu_window_command_t g_cmd_window_interior_command;
extern s16 g_menu_window_right_x;

/* scrollable lists */
/* A scrollable list shows the rows of a -1 terminated entry list by name from a text table, under the geometry of
 * a list record (menu_list_record_t). Each program draws the rows with its own
 * *_menu_opcode_10_draw_scrollable_list. */

/* Page scroll direction of the overlay unit, item and job lists. */
typedef enum menu_vertical_scroll_direction {
    MENU_VERTICAL_SCROLL_UP = -1,
    MENU_VERTICAL_SCROLL_NONE = 0,
    MENU_VERTICAL_SCROLL_DOWN = 1,
    MENU_VERTICAL_SCROLL_WAIT = 2,
} menu_vertical_scroll_direction_e;

extern s16* g_menu_list_entries;
extern s16 g_menu_list_entry_count;
extern u8* g_menu_list_text_table;
extern s16 g_menu_list_selected_index;
extern s16 g_menu_scroll_base_index;
extern s16 g_menu_list_visible_rows;
extern s16 g_menu_list_row_height;
extern s16 g_menu_list_row_group_count;
extern u8 g_menu_list_glyph_width;
extern u8 g_menu_list_text_columns;
extern u16 g_menu_list_vram_width;
extern s8 g_menu_list_scroll_direction; /* read with lb */
extern u16 g_menu_list_scroll_progress;
extern s8 g_menu_list_redraw_pending; /* read with lb (the per-program *_menu_opcode_10_draw_scrollable_list copies) */
/* The row being drawn, its row mode and its scroll offset, which the row instructions read. */
extern s32 g_menu_list_row_index;
extern s16 g_menu_list_row_mode;
extern s16 g_menu_list_scroll_offset;

void menu_init_scrollable_list(const s16* entries, s32 selected_index, s32 scroll_base_index, u8* text_table);
/* BUNIT, EQUIP, JOBSTTS and WORLD each build their own body. */
void menu_init_scrollable_list_core(const s16* entries, s32 selected_index, u8* text_table);
void menu_draw_scrollable_list(const menu_instruction_t* script);
void menu_init_and_draw_scrollable_list(
    const s16* entries, s32 selected_index, s32 scroll_base_index, u8* text_table, const menu_instruction_t* script);
void menu_init_and_draw_scrollable_list_simple(
    s16* entries, s32 selected_index, u8* text_table, const menu_instruction_t* script);
void menu_scroll_list_by_page(menu_vertical_scroll_direction_e direction, const menu_instruction_t* list);

#endif
