#ifndef MENU_H
#define MENU_H

/* The shared menu core: menu entries and their context records, menu frame timing and scene state, the menu arena,
 * equipment-slot and unit-grid readout choices, icon and number records, and common panel drawing (gradient panels,
 * scaled draw areas, panel fades). */

#include "fft/gfx/gfx.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* frame timing */
/* Raw VSync mode; animation and script steps use 1 when this is zero. */
extern s8 g_menu_event_speed;

s32 menu_get_event_speed(void);
void menu_set_event_speed(s32 speed);
s32 menu_update_event_frame(u32* otag, s32 controller_input);

/* menu arena */
void* menu_alloc_memory(s32 bytes);
void menu_free_memory(void* buffer);
void menu_init_buffer_allocator(void);
void* menu_alloc_buffer(s32 bytes);

/* menu entries */
/* A scene's menus are a table of menu entries, one per window: its size and text image, its rows and their
 * actions, and the thread that runs it. The thread receives the entry as its first parameter and reads its
 * own record type through context. */

/* Row actions 0..0xff open that menu entry; this flag suppresses the confirm sound. */
enum { MENU_ROW_ACTION_SILENT = 0x1000 };

/* Special row actions. CLEAR_SELECTION records no selection without opening another entry; NONE,
 * like any other value from 0x100 up, opens nothing, so Circle only records the row. */
enum { MENU_ROW_ACTION_CLEAR_SELECTION = 0x4d2, MENU_ROW_ACTION_NONE = 0x7d0 };

/* Header sprites selected by menu_entry_t.header_id in the icon-list threads.
 * The names are the labels in FRAME.BIN; NONE places its label off screen. */
typedef enum menu_icon_header {
    MENU_ICON_HEADER_NONE = 0,
    MENU_ICON_HEADER_MENU = 1,
    MENU_ICON_HEADER_CHECK = 2,
    MENU_ICON_HEADER_EFFECT = 3,
    MENU_ICON_HEADER_TURN = 4,
    MENU_ICON_HEADER_ABILITY_TURN_REFLECT = 5,
    MENU_ICON_HEADER_ABILITY_MP_TURN_REFLECT = 6,
    MENU_ICON_HEADER_STOCK_TURN_REFLECT = 7,
    MENU_ICON_HEADER_TURN_REFLECT = 8,
    MENU_ICON_HEADER_ABILITY = 9,
    MENU_ICON_HEADER_MENU_WITH_ARROW = 16
} menu_icon_header_e;

/* One menu window, shared by BATTLE, WORLD and the overlays; g_menu_thread_menu_data points at the scene's
 * current table. The icon threads read vram_origin and the inner size as the text image's upload rectangle and
 * window.x/y as the icon origin (the icon list thread clears bit 0 of window.y);
 * menu_update_row_cursor wraps the cursor at max_row_index. */
typedef struct menu_entry {
    point16_t vram_origin; /* 0x00: text-image VRAM origin passed to the draw-mode helpers */
    s16 inner_width;       /* 0x04: text-image width; RECT.w = inner_width >> 2 (4bpp) */
    s16 inner_height;      /* 0x06: text-image height (size_entry_to_text) */
    RECT window;           /* 0x08: screen position and extent passed to image and draw-area helpers */
    /* 0x10: text sprite placement (menu_build_window_sprites). width/height are set with the
     * inner and window sizes; the load origin and screen offsets are cleared
     * with them. */
    image_layout_t text_image;
    s16 text_id;                 /* 0x1c: passed to text_find_entry */
    s16 max_row_index;           /* 0x1e: row count - 1 (build_act_menu) */
    s16 thread_count;            /* 0x20: threads menu_handle_cancel_input stops, -1 when there is no thread
                                    group; set from the system-function table by request_thread */
    u8 _padding_22[0x24 - 0x22]; /* aligns row_actions */
    s16* row_actions;            /* 0x24: each row's action, indexed by selected_index */
    void (*thread_entry)(void);  /* 0x28 */
    s16 header_id;               /* 0x2c: text-thread parameter (menu_screen_text_entry_thread);
                                    icon threads use menu_icon_header_e */
    u8 _padding_2e[2];           /* aligns context */
    /* 0x30: context interpreted by the menu thread. Text threads use menu_text_binding_t
     * here, the list threads a menu_text_layout_t, the number threads a
     * menu_screen_number_range_t and the icon threads the s16 redraw flag they
     * poll (set to 1 to rebuild the text image). */
    void* context;
    void* value;           /* 0x34: thread-specific value data: s16 result cell or main-window s32 substitution pair */
    s16 selected_index;    /* 0x38: selected row; indexes the scroll layout's 16-bit table
                              (menu_screen_help_text_thread) */
    s16 select_text_table; /* 0x3a: "Index for select text": row of g_menu_screen_entry_text_id_tables
                              (0x11 = first text column of the scroll layout); 2 / -1 defer to the next running
                              thread's entry. The AT-list and option-list builders park their
                              scroll row offset here instead. */
} menu_entry_t;
typedef char menu_entry_size_must_be_60[(sizeof(menu_entry_t) == 60) ? 1 : -1];

/* icons and numbers */
/* Texture rectangle, CLUT and texture page of an icon, as the menu row callbacks return it. */
typedef struct menu_icon_source {
    s16 u;     /* 0x00 */
    s16 v;     /* 0x02 */
    s16 w;     /* 0x04 */
    s16 h;     /* 0x06 */
    u16 clut;  /* 0x08 */
    u16 tpage; /* 0x0a */
} menu_icon_source_t;
typedef char menu_icon_source_size_must_be_12[sizeof(menu_icon_source_t) == 12 ? 1 : -1];

/* One positioned number: a text position, a pointer to the live value and its format.
 * text_draw_number_entries selects small glyphs for kind 0 and large glyphs for kind 1.
 * The status panels' decimal entry renderers ignore kind and interpret format separately. */
typedef struct menu_number_entry {
    u16 x;      /* 0x00 */
    u16 y;      /* 0x02 */
    s16* value; /* 0x04 */
    s16 format; /* 0x08; count and flags of the selected renderer (fft/text.h) */
    s16 kind;   /* 0x0a */
} menu_number_entry_t;
typedef char menu_number_entry_size_must_be_12[(sizeof(menu_number_entry_t) == 12) ? 1 : -1];

/* scene state */
extern s32 g_menu_post_battle_unit_changes_active;
extern u8 g_menu_common_graphics_loaded;

/* The field y (0 or 0xf0) handed to menu_input_update_event_state each frame; the panels draw into the other
 * frame-buffer half. */
extern u16 g_menu_framebuffer_y;

/* 0: normal, 1: companion/editor transition, 2: AT-list selection. */
extern u16 g_menu_overlay_state;

/* Nonzero while a screen transition holds the menus (4 during the unit list fade-out). */
extern u16 g_menu_transition_state;

/* Loads the default menu palettes and rebuilds the text section pointers. */
void menu_init_palette_text(s32 unused_0);

/* Binds the scene's working pointers and clears its menu-session flags. */
void menu_init_subsystem_pointers(void);
void menu_subsystems_init(void);

/* Empty; menu_subsystems_init calls it between the palette and text setup and the subsystem reset. */
void menu_init_stub(void);

/* Frees every window buffer pair, the tpage-7 VRAM grid and the menu arena, and resets the thread scheduler. */
void menu_subsystems_reset(void);

/* 2 while a menu is being built, 4 while a menu thread runs, 0 when idle; BATTLE's body first returns
 * g_menu_transition_state when it is set. */
s32 menu_thread_get_activity(void);

/* equipment slots */
/* The five equipment slots in the order the equipment screens list them (not
 * the order of a unit record's equipment array). */
typedef enum menu_equipment_slot {
    MENU_EQUIPMENT_SLOT_RIGHT_HAND = 0,
    MENU_EQUIPMENT_SLOT_LEFT_HAND = 1,
    MENU_EQUIPMENT_SLOT_HEAD = 2,
    MENU_EQUIPMENT_SLOT_BODY = 3,
    MENU_EQUIPMENT_SLOT_ACCESSORY = 4,
    MENU_EQUIPMENT_SLOT_HAND_COUNT = 2,
    MENU_EQUIPMENT_SLOT_COUNT = 5
} menu_equipment_slot_e;

/* unit grid readouts */
/* What the unit grids draw below a unit (draw_unit_readout,
 * unit_grid_update). L2/R2 step g_formation_stat_display_mode and
 * g_unit_list_stat_mode through the first five, wrapping between HP and
 * BRAVE_FAITH; WORLD picks the others per unit. */
typedef enum menu_unit_readout {
    MENU_UNIT_READOUT_HP = 0,
    MENU_UNIT_READOUT_MP = 1,
    MENU_UNIT_READOUT_CT = 2,
    MENU_UNIT_READOUT_LEVEL_EXP = 3,
    MENU_UNIT_READOUT_BRAVE_FAITH = 4,
    MENU_UNIT_READOUT_EQUIP_LABEL = 8, /* "equipping", or "can't equip" below a value of 100 */
    MENU_UNIT_READOUT_LEAVING = 9,     /* away on a proposition */
    MENU_UNIT_READOUT_EGG = 10,        /* nothing is drawn */
    MENU_UNIT_READOUT_MISSING = 11
} menu_unit_readout_e;

/* gradient panels */
/* Double-buffered panel packets: a center tile and nine-scanline top and
 * bottom gradients, used by status indicators and menu panel fades. */
typedef struct menu_gradient_panel_prims {
    DR_MODE draw_mode; /* 0x00 */
    TILE center;       /* 0x0c */
    TILE top[9];       /* 0x1c */
    TILE bottom[9];    /* 0xac */
} menu_gradient_panel_prims_t;
typedef char menu_gradient_panel_prims_size_must_be_316[(sizeof(menu_gradient_panel_prims_t) == 316) ? 1 : -1];

/* scaled draw areas */
/* A panel that zooms open draws through a pair of draw areas: the panel's rectangle at the thread's position,
 * cropped about its center to the percentage scaled_draw_area.c's scale table gives for scale_index, and the frame
 * buffer's own area. */
extern RECT g_menu_draw_area_template;

/* Nonzero upper_half selects the buffer at y 0; zero selects the one at y 240. */
void menu_build_scaled_draw_area_packets(
    scaled_draw_area_pair_t* packet, const RECT* source, s32 scale_index, s32 upper_half, const point32_t* position);

/* Clears the first draw area's width and installs the default second draw area. */
void menu_init_scaled_draw_area_packets(scaled_draw_area_pair_t* data);

/* panel fades */
/* g_menu_panel_fade_mode: what menu_update_panel_fade does to the panel's
 * dimming tiles. */
typedef enum menu_panel_fade_mode {
    MENU_PANEL_FADE_NONE = 0, /* not drawn */
    MENU_PANEL_FADE_IN = 1,   /* intensity rises by 8 a frame up to 80 and holds */
    MENU_PANEL_FADE_OUT = 2   /* intensity falls by 8 a frame; at zero the mode returns to NONE */
} menu_panel_fade_mode_e;

extern s32 g_menu_panel_fade_mode; /* menu_panel_fade_mode_e */
extern s32 g_menu_panel_fade_intensity;

void menu_update_panel_fade(void);

/* A black semi-transparent 256x50 center tile with nine one-line tiles above and below for the graded edges. */
void menu_init_panel_fade_tiles(void);

#endif
