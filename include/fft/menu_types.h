#ifndef FFT_MENU_TYPES_H
#define FFT_MENU_TYPES_H

#include "psx/gpu.h"
#include "psx/types.h"

/* Per-skillset menu behavior stored at 0x80065cb4. Values select specialized
 * inventory, targeting, and ability-loading paths; they are not menu command
 * opcodes. */
typedef enum action_menu_type {
    ACTION_MENU_TYPE_DEFAULT = 0x00,
    ACTION_MENU_TYPE_ITEM_INVENTORY = 0x01,
    ACTION_MENU_TYPE_WEAPON_INVENTORY = 0x02,
    ACTION_MENU_TYPE_ARITHMETICKS = 0x03,
    ACTION_MENU_TYPE_ELEMENTS = 0x04,
    ACTION_MENU_TYPE_BLANK_05 = 0x05,
    ACTION_MENU_TYPE_MONSTER = 0x06,
    ACTION_MENU_TYPE_KATANA_INVENTORY = 0x07,
    ACTION_MENU_TYPE_ATTACK = 0x08,
    ACTION_MENU_TYPE_JUMP = 0x09,
    ACTION_MENU_TYPE_CHARGE = 0x0a,
    ACTION_MENU_TYPE_DEFEND = 0x0b,
    ACTION_MENU_TYPE_CHANGE_EQUIPMENT = 0x0c,
    ACTION_MENU_TYPE_UNKNOWN_0D = 0x0d,
    ACTION_MENU_TYPE_BLANK_0E = 0x0e,
    ACTION_MENU_TYPE_UNKNOWN_0F = 0x0f,
    ACTION_MENU_TYPE_COUNT = 0x10,
} action_menu_type_e;

/* Contextual skillset IDs synthesized by the action menu. The value 0xbc is
 * also an ordinary monster skillset when it comes from a monster unit. */
typedef enum action_menu_pseudo_skillset {
    ACTION_MENU_PSEUDO_SKILLSET_ANYTHING = 0xbc,
} action_menu_pseudo_skillset_e;

/* Menu and graphics records shared by the BATTLE and WORLD menu code. */
typedef LINE_F2 world_menu_line_f2_t;

/* Seven-byte window command in the menu script streams. The length field
 * advances to the next command, which may have a different size. */
typedef struct world_menu_window_command {
    u8 opcode;
    u8 length;
    u8 rectangle_source;
    u8 x;
    u8 y;
    u8 width;
    u8 height;
} world_menu_window_command_t;

/* Image load parameters consumed by world_gfx_init_image_loading (0xc bytes;
 * the table at 0x80155838 holds one per menu sprite). */
typedef struct world_gfx_image_load_parameters {
    s16 x_load;
    s16 y_load;
    u16 width;
    u16 height;
    u16 x_screen_offset;
    u16 y_screen_offset;
} world_gfx_image_load_parameters_t;

/* Base texture/screen location paired with the parameters above by
 * world_gfx_init_image_loading: the x is sign-tested (an s16) while the y is
 * only ever added as an unsigned VRAM row. Hoisted here from the three
 * identical local copies in the WORLD and BATTLE unit-editor panels. */
typedef struct world_image_location {
    s16 x; /* 0x00 */
    u16 y; /* 0x02 */
} world_image_location_t;

/* Menu line-box layout, established by the builder at 0x800e3358: two DR_MODE
 * packets, twelve LINE_F2 packets and one SPRT whose CLUT (+0xe6) the
 * palette-bank initialisers set. */
typedef struct world_menu_palette_primitives {
    DR_MODE draw_mode;              /* 0x00: texture window g_world_gfx_texture_window */
    DR_MODE draw_mode_menu;         /* 0x0c: texture window g_world_menu_numeric_display_texture_window */
    world_menu_line_f2_t lines[12]; /* 0x18 */
    SPRT sprite;                    /* 0xd8 */
} world_menu_palette_primitives_t;

/* One 0x100-byte slot holding a panel's palette primitive set. */
typedef union world_menu_palette_slot {
    world_menu_palette_primitives_t primitives;
    u8 storage[0x100];
} world_menu_palette_slot_t;

/* 0x800ef2fc initializes this 0x13c-byte packet group and copies it to the
 * second frame buffer. Each side contains nine tiles, ordered left to right. */
typedef struct world_fade_tile_frame {
    DR_MODE draw_mode;
    TILE center;
    TILE sides[18];
} world_fade_tile_frame_t;

/* Provisional: menu text origin/stride words at 0x80153280 (the WORLD
 * script variable region; world_menu_set_text_origin writes +0x04/+0x06).
 * The text-image builders address these through one base register, so they
 * are modelled as one aggregate; the two parameter words at 0x80153304 and
 * 0x80153308 schedule as plain scalars and stay separate globals. */
/* The world state block at 0x80153280. Its first word is the script-variable
 * pointer, which world_init_scene_bindings points at g_word_flags; code that
 * only needs that word binds the same address as g_world_script_variables. */
typedef struct world_menu_text_state {
    s32* script_variables; /* 0x00; also bound as g_world_script_variables */
    s16 origin_x;          /* 0x04 (0x80153284) */
    s16 origin_y;          /* 0x06 (0x80153286) */
    s32 unknown_08;
    s32 stride; /* 0x0c (0x8015328c): rect width * 4 */
    s32 color;  /* 0x10 (0x80153290): glyph blit fill; 0x44444444 while text colour 4 is drawn
                   (world_menu_draw_text_columns) */
} world_menu_text_state_t;

/* The BATTLE twin of the block above, at 0x80165f9c: same layout, but its
 * first word is not the script-variable pointer.  Named separately so the
 * BATTLE object does not have to borrow the WORLD record's type. */
typedef struct menu_text_state {
    s32 unknown_00; /* 0x00 (0x80165f9c) */
    s16 origin_x;   /* 0x04 (0x80165fa0): battle_menu_set_text_origin */
    s16 origin_y;   /* 0x06 (0x80165fa2) */
    s32 unknown_08; /* 0x08 (0x80165fa4) */
    s32 stride;     /* 0x0c (0x80165fa8): g_menu_inner_window_width */
    s32 color;      /* 0x10 (0x80165fac): glyph blit fill; 0x44444444 while text colour 4 is drawn */
} menu_text_state_t;

/* Provisional: 0x3c-byte menu entry reached through
 * g_world_menu_thread_menu_data; the same menu record as BATTLE
 * g_battle_menu_thread_menu_data and the OPTION/EQUIP menus. 0x0c/0x0e are the
 * window size (frame rect 0x0c - 0x12 by 0x0e - 0x1a in WORLD 0x8010088c and
 * option_build_at_list). The parent table at 0x24 (indexed by selected_index)
 * and the thread entry at 0x28 are known from
 * world_menu_start_entry_chain_threads. */
typedef struct world_menu_entry {
    s16 vram_x;         /* 0x00: text-image VRAM x (world_menu_redraw_text_page_on_scroll) */
    s16 vram_y;         /* 0x02: text-image VRAM y */
    s16 inner_width;    /* 0x04: text-image width; RECT.w = inner_width >> 2 (4bpp) */
    s16 inner_height;   /* 0x06: text-image height (world_menu_size_entry_to_text) */
    u16 window_x;       /* 0x08: screen x; shifted left as the width grows */
    s16 window_y;       /* 0x0a: screen y; (7 - visible rows) * 8 - 0x48 in world_menu_build_available_item_list */
    s16 window_width;   /* 0x0c: window width; scroll arrows at window_x + window_width - 0xf */
    s16 window_height;  /* 0x0e: window height; child windows open at window_y + window_height */
    s16 unknown_10[2];  /* 0x10: cleared with the size fields (open_menu_start_music_test_list_thread) */
    s16 overall_width;  /* 0x14: set with inner_width and window_width */
    s16 overall_height; /* 0x16: set with inner_height and window_height */
    s16 unknown_18[2];  /* 0x18: cleared with the size fields */
    s16 text_id;        /* 0x1c: passed to world_text_find_entry */
    s16 max_row_index;  /* 0x1e: row count - 1 (world_menu_build_skillset_entries) */
    u16 field_0x20;     /* 0x20: set from the system-function table by world_menu_run_system_function_thread */
    u8 _pad22[0x24 - 0x22];
    s16* parent_indices;        /* 0x24 */
    void (*thread_entry)(void); /* 0x28 */
    u16 header_id;              /* 0x2c: menu header; text-thread parameter (world_menu_text_entry_thread) */
    u8 _pad2e[2];
    struct world_menu_text_binding* text_binding; /* 0x30 */
    s16* value;            /* 0x34: result cell (g_world_menu_selected_option); the number-entry threads edit it */
    s16 selected_index;    /* 0x38: selected row; indexes the binding's halfword table
                              (world_menu_announce_entry_value_thread) */
    s16 select_text_table; /* 0x3a: "Index for select text": row of g_world_menu_entry_text_id_tables
                              (0x11 = the binding's row_text_ids); 2 / -1 defer to the next running
                              thread's entry. The AT-list and option-list builders park their
                              scroll row offset here instead. */
} world_menu_entry_t;

/* BATTLE's current menu-entry table pointer at 0x80173cb8 (menu threads are
 * started on &table[i]): battle_menu_init_subsystem_pointers points it at
 * g_battle_menu_idle_action_entries, and the event overlays that run over
 * BATTLE swap in their own tables and restore it. WORLD's twin is
 * g_world_menu_thread_menu_data. */
extern world_menu_entry_t* g_battle_menu_thread_menu_data;

/* Provisional: three-column menu text layout walked by world_menu_draw_text_columns (0x80109374) /
 * world_menu_draw_text_columns_2 (reached through world_menu_entry_t::text_binding when
 * that record is a layout rather than a binding). Column i uses x[i],
 * mode[i] (0 text, 1 number, 2 hidden, 3 alternate number), text_ids[i] and
 * text_colors[i], each indexed by row + row offset. */
typedef struct world_menu_text_layout {
    s16 row_count;       /* 0x00 */
    s16 hidden_rows;     /* 0x02: rows beyond the visible page (count - visible, 0 when
                            all fit); row_offset is clamped to it */
    s16 x[3];            /* 0x04 */
    s16 mode[3];         /* 0x0a */
    s16* text_ids[3];    /* 0x10 */
    s16* text_colors[3]; /* 0x1c */
} world_menu_text_layout_t;

typedef char world_menu_text_layout_size_must_be_0x28[sizeof(world_menu_text_layout_t) == 0x28 ? 1 : -1];

/* One selectable save-file icon: 16-colour CLUT plus 16x16 4bpp image. */
typedef struct card_save_icon {
    u8 clut[0x20];  /* 0x00 */
    u8 image[0x80]; /* 0x20 */
} card_save_icon_t;

/* One cursor-navigation node of the HELPMENU, BUNIT, ATTACK deployment and
 * DEBUGCHR editor screens (WORLD's world_help_navigation_entry_t has the same
 * layout): the value or text id it refers to, its help-text base, the node
 * reached for each d-pad direction (up 0x1000, down 0x4000, right 0x2000,
 * left 0x8000) and the cursor position. */
typedef struct help_navigation_record {
    s16* value;          /* 0x00 */
    s16 help_base;       /* 0x04 */
    u8 destination[4];   /* 0x06: up, down, right, left */
    s16 x;               /* 0x0a */
    s16 y;               /* 0x0c */
    s16 vertical_cursor; /* 0x0e: nonzero: the cursor points down and bobs along y */
} help_navigation_record_t;

typedef char help_navigation_record_size_must_be_0x10[(sizeof(help_navigation_record_t) == 0x10) ? 1 : -1];

/* Provisional: one double-buffered menu sprite page (0xa0 bytes) built by
 * world_menu_build_sprite_page and its BATTLE twin battle_menu_build_sprite_page:
 * five DR_MODE texture-window packets followed by the five SPRTs they apply
 * to. Both modules keep two pages at 0x801cc074. */
typedef struct menu_sprite_page {
    DR_MODE modes[5]; /* 0x00 */
    SPRT sprites[5];  /* 0x3c */
} menu_sprite_page_t;

typedef char menu_sprite_page_size_must_be_0xa0[(sizeof(menu_sprite_page_t) == 0xa0) ? 1 : -1];

#endif
