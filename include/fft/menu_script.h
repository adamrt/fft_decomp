#ifndef MENU_SCRIPT_H
#define MENU_SCRIPT_H

/* Byte-aligned display instructions shared by WORLD, BUNIT, EQUIP and JOBSTTS. */

#include "fft/menu.h"

/* opcodes */
/* Opcode byte of a menu script command, the index of its handler in the
 * handler table of WORLD, BUNIT, EQUIP and JOBSTTS (CARD calls the same
 * handlers directly). Only the opcodes the code tests are named. Each
 * module ends a stream with its own opcode, the first one past its table
 * (WORLD_MENU_SCRIPT_END, BUNIT_CMD_END, EQUIP_CMD_END, JOBSTTS_CMD_END). */
typedef enum menu_script_opcode {
    MENU_SCRIPT_OP_IF_GROUP = 0x01,        /* *_cmd_run_if_group_handler */
    MENU_SCRIPT_OP_IF_ELSE_GROUP = 0x02,   /* *_cmd_run_if_else_group_handler */
    MENU_SCRIPT_OP_UNCOLORED_QUAD = 0x04,  /* *_cmd_draw_textured_quad_handler; opcode 5 applies the draw color */
    MENU_SCRIPT_OP_SCROLLABLE_LIST = 0x10, /* the list geometry *_menu_draw_scrollable_list reads */
    /* The "draw number" opcodes of the WORLD and BUNIT scripts that
     * *_menu_script_draw_formatted_number tells apart from its plain form (0x0d). */
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
 * value, read by *_menu_script_draw_formatted_number and the overlays'
 * *_cmd_draw_formatted_number_handler. */
typedef enum menu_number_value_flags {
    MENU_NUMBER_VALUE_DASHES = 0x20000000,   /* a row of dashes instead of the digits */
    MENU_NUMBER_VALUE_DISABLED = 0x40000000, /* drawn with the disabled number clut */
} menu_number_value_flags_e;

/* Ten-byte menu-script "draw number" command: an
 * opcode/length prefix followed by the value getter (a row-callback index),
 * layout and byte position operands. world_menu_script_draw_formatted_number
 * and the overlay copies advance by `length`. */
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
 * world_menu_load_scrollable_list_layout reads and the placement
 * world_menu_script_draw_scrollable_list draws with. */
typedef struct menu_list_record {
    u8 opcode;          /* 0x00: 0x10 = list layout */
    u8 length;          /* 0x01: byte length of this record */
    u8 show_cursor;     /* 0x02 */
    u8 _unused_03;      /* 0x03 */
    u8 skip_count;      /* 0x04: row records that follow (g_world_menu_list_row_record_count) */
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
    u8 _unknown_02;
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

/* drawing color */
menu_instruction_t* menu_cmd_set_draw_color_handler(menu_instruction_t* command);

/* command dispatch */
extern menu_callback_t g_menu_row_callbacks[16];
extern s16 g_menu_list_row_mode;
extern s16 g_menu_list_scroll_offset;
extern s32 g_menu_list_row_index;

menu_instruction_t* menu_cmd_run_if_group_handler(menu_instruction_t* command);
menu_instruction_t* menu_cmd_run_if_else_group_handler(menu_instruction_t* command);
menu_window_command_t* menu_cmd_draw_tiled_rectangle_handler(menu_window_command_t* command);
void menu_cmd_run_stream(menu_instruction_t* instruction, s32 flags);
extern u32 g_menu_cmd_stream_input;
extern menu_instruction_t* (*g_menu_cmd_handlers[])(menu_instruction_t* instruction);
menu_instruction_t* menu_cmd_advance_window_scale_step_handler(menu_instruction_t* instruction);
menu_instruction_t* menu_cmd_enqueue_draw_mode_handler(menu_instruction_t* instruction);
menu_instruction_t* menu_cmd_release_otag_lock_handler(menu_instruction_t* instruction);
menu_instruction_t* menu_cmd_draw_row_callback_quad_handler(menu_instruction_t* instruction);
extern menu_sprite_command_t g_menu_cmd_row_quad;

/* palette commands */
menu_instruction_t* menu_cmd_set_clut_handler(menu_instruction_t* command);
menu_instruction_t* menu_cmd_set_layout_clut_handler(menu_instruction_t* command);
menu_instruction_t* menu_cmd_set_tpage_handler(menu_instruction_t* command);

/* window drawing */
extern menu_window_command_t g_menu_window_interior_command;
extern s16 g_menu_window_right_x;

menu_instruction_t* menu_cmd_draw_background_tiles_handler(menu_instruction_t* instruction);
menu_instruction_t* menu_cmd_draw_window_frame_handler(menu_instruction_t* command);
menu_instruction_t* menu_cmd_draw_textured_quad_handler(menu_instruction_t* instruction);

/* draw area commands */
menu_instruction_t* menu_cmd_add_next_otag_draw_area_handler(menu_instruction_t* instruction);
menu_instruction_t* menu_cmd_set_draw_area_if_unlocked_handler(menu_instruction_t* instruction);
menu_instruction_t* menu_cmd_set_zoomed_draw_area_handler(menu_instruction_t* instruction);

/* instruction cursors */
menu_instruction_t* menu_cmd_skip_group(menu_instruction_t* command);

static __inline__ menu_instruction_t* menu_next_instruction(menu_instruction_t* instruction) {
    return (menu_instruction_t*)((u8*)instruction + instruction->header.length);
}

static __inline__ const menu_instruction_t* menu_next_const_instruction(const menu_instruction_t* instruction) {
    return (const menu_instruction_t*)((const u8*)instruction + instruction->header.length);
}

/* list scripts */
void menu_draw_scrollable_list(const menu_instruction_t* script);
void menu_scroll_list_by_page(menu_vertical_scroll_direction_e direction, const menu_instruction_t* list);
void menu_init_and_draw_scrollable_list(
    s16* entries, s32 selected_index, s32 scroll_base_index, u8* text_table, const menu_instruction_t* script);
void menu_run_script_with_palette_mode(menu_instruction_t* script, s32 state, s32 override);
void menu_init_and_draw_scrollable_list_simple(
    s16* entries, s32 selected_index, u8* text_table, const menu_instruction_t* script);

#endif /* MENU_SCRIPT_H */
